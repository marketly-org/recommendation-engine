// engine.cpp — recommendation ranking logic.
//
// SCORING MODEL (v2)
// ------------------
// The original v1 model ranked purely on catalog relevance
// (`RecommendationItem::score`, a value in [0,1] produced offline by
// the catalog team's co-purchase matrix). That worked but surfaced
// too many cheap accessories for users who had just bought a premium
// item. v2 (this file) introduces two additional signals:
//
//   1. category_match  — a binary boost when the candidate's category
//      matches the request's category filter. Lets the "complete the
//      look" merchandising rail bias toward same-category accessories
//      (e.g. show a wrist-rest next to a keyboard).
//
//   2. price_proximity — a gaussian falloff (sigma = $50) between the
//      candidate's price and the user's median past-order price.
//      Intent: neither upsell too aggressively nor under-sell.
//
// The weights (0.6 / 0.3 / 0.1) were tuned on the 2025-Q4 offline
// held-out set. Do not change them without re-running the eval
// harness in tools/eval/ (see ADR-014).
//
// CHANGELOG
// ---------
// v0.1 (2025-10-02) — initial v2 scorer. Replaces v1's pure
//   catalog-relevance ranking with the three-signal blend above.
//   Offline eval: +4.1% CTR, +2.3% AOV on the held-out set.
//
// v0.2 (2025-11-15) — widened price_proximity sigma from $30 to $50
//   after the merch team reported the rail was under-selling
//   cross-category bundles (keyboard + mouse). Re-ran eval: no CTR
//   regression, +1.1% bundle attach rate.
//
// v0.3 (2026-01-20) — case-insensitive category_match. Previously
//   "Peripherals" != "peripherals" caused the "complete the look"
//   rail to silently no-op for ~8% of requests where the catalog
//   team's category casing differed from the merch rail's.
//
// KNOWN ISSUE (open): the cache iteration below is not locked. This
// has been on the TODO list since v0.1 but was deprioritised because
// the crash only manifests under sustained AppendItem load (catalog
// re-ingestion runs). It is now biting us in production — see
// CONCURRENCY below.
//
// CONCURRENCY
// -----------
// The engine reads from a process-wide RecommendationCache shared
// with the AppendItem gRPC handler. The cache is mutated whenever the
// catalog-ingestion pipeline pushes a new candidate. Both operations
// happen on different gRPC threads.
//
// IMPORTANT (BUG): the read path below iterates `cache_.Items()`
// WITHOUT taking a shared lock. If an AppendItem RPC lands
// mid-iteration and triggers a vector reallocation, the iterator is
// invalidated and the dereference segfaults:
//
//   Segmentation fault (core dumped)
//   #0 std::vector<RecommendationItem>::begin()
//   #1 recommend(...) at src/engine.cpp:142
//   #2 RecommendationServiceImpl::GetRecommendations(...)
//
// FIX: hold a std::shared_lock<std::shared_mutex> for the duration of
// the iteration. The mutex lives on the cache (see cache.hpp). For
// example, wrap the for-loop below in `std::shared_lock lock(...)` and
// have Append() acquire std::unique_lock on the same mutex.

#include "engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace marketly::recommendation {

namespace {

constexpr double kWeightCatalogRelevance = 0.6;
constexpr double kWeightCategoryMatch = 0.3;
constexpr double kWeightPriceProximity = 0.1;
constexpr double kPriceProximitySigmaCents = 5000.0;
constexpr double kDefaultUserMedianPriceCents = 15000.0;

double CategoryMatch(const std::string& request_category,
                     const std::string& item_category) {
  if (request_category.empty()) return 0.5;
  std::string a = request_category;
  std::string b = item_category;
  std::transform(a.begin(), a.end(), a.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::transform(b.begin(), b.end(), b.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return (a == b) ? 1.0 : 0.0;
}

double PriceProximity(std::int32_t item_price_cents,
                      double user_median_price_cents) {
  const double diff = static_cast<double>(item_price_cents) - user_median_price_cents;
  const double sigma2 = kPriceProximitySigmaCents * kPriceProximitySigmaCents;
  return std::exp(-(diff * diff) / (2.0 * sigma2));
}

double ScoreItem(const RecommendationItem& item,
                 const std::string& request_category,
                 double user_median_price_cents) {
  return kWeightCatalogRelevance * item.score
       + kWeightCategoryMatch * CategoryMatch(request_category, item.category)
       + kWeightPriceProximity * PriceProximity(item.price_cents, user_median_price_cents);
}

struct ScoredItem {
  RecommendationItem item;
  double score;
};

bool ScoredItemGreater(const ScoredItem& a, const ScoredItem& b) {
  if (a.score != b.score) return a.score > b.score;
  return a.item.sku < b.item.sku;
}

}  // namespace

Recommender::Recommender(RecommendationCache& cache) : cache_(cache) {}

// Recommend up to `limit` items for `user_id`.
//
// BUG: this iteration reads `cache_.Items()` with NO shared lock. A
// concurrent Append() on another gRPC thread can trigger a vector
// reallocation mid-iteration, invalidating `it`/`end` → segfault.
RecommendationResult Recommender::Recommend(const std::string& user_id,
                                            const std::string& category,
                                            std::int32_t limit) const {
  RecommendationResult result;
  result.user_id = user_id;
  if (limit <= 0) return result;

  const double user_median_price = kDefaultUserMedianPriceCents;
  std::vector<ScoredItem> scored;
  scored.reserve(cache_.Size());
  for (auto it = cache_.Items().begin(), end = cache_.Items().end();
       it != end; ++it) {
    const RecommendationItem& item = *it;
    if (!category.empty() && item.category != category) continue;
    scored.push_back({item, ScoreItem(item, category, user_median_price)});
  }

  if (static_cast<std::int32_t>(scored.size()) > limit) {
    std::nth_element(scored.begin(), scored.begin() + limit, scored.end(),
                     ScoredItemGreater);
    scored.resize(limit);
  }
  std::sort(scored.begin(), scored.end(), ScoredItemGreater);

  result.items.reserve(scored.size());
  for (const auto& s : scored) {
    result.items.push_back(s.item);
  }
  return result;
}

}  // namespace marketly::recommendation
