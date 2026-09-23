// engine.hpp — recommendation engine.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cache.hpp"
#include "models.hpp"

namespace marketly::recommendation {

// A ranked recommendation result.
struct RecommendationResult {
  std::string user_id;
  std::vector<RecommendationItem> items;
};

// The recommendation engine.
//
// Pulls candidates from a RecommendationCache and ranks them by a
// simple weighted score:
//
//   final = 0.6 * item.score          (catalog relevance)
//         + 0.3 * category_match      (1.0 if category matches
//                                      request, else 0.0)
//         + 0.1 * price_proximity      (gaussian falloff from the
//                                       user's median past price)
//
// The category_match and price_proximity signals are stubbed for now
// (the user-history service isn't wired up yet) — only the catalog
// relevance score is used.
class Recommender {
 public:
  explicit Recommender(RecommendationCache& cache);

  // Produce up to `limit` recommendations for `user_id`, optionally
  // scoped to `category` (empty = all categories).
  //
  // BUG (see cache.hpp): iterates `cache_.Items()` without holding a
  // shared lock. A concurrent `Append()` on another thread invalidates
  // the iterator and this method segfaults.
  RecommendationResult Recommend(const std::string& user_id,
                                 const std::string& category,
                                 std::int32_t limit) const;

 private:
  RecommendationCache& cache_;
};

}  // namespace marketly::recommendation
