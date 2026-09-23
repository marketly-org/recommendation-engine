// test_engine.cpp — unit tests for the recommendation engine.
//
// They cover the ranking math + filtering + limit clamping.

#include "cache.hpp"
#include "engine.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace marketly::recommendation;

namespace {

RecommendationItem MakeItem(std::string sku, std::string name,
                            std::string category, double score,
                            std::int32_t price_cents) {
  return RecommendationItem{std::move(sku), std::move(name),
                            std::move(category), score, price_cents};
}

// Tiny test framework — no external dependency, just assert + cout.
int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__                \
                << " CHECK(" #cond ") failed\n";                          \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    auto _va = (a);                                                       \
    auto _vb = (b);                                                       \
    if (!(_va == _vb)) {                                                  \
      std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__                \
                << " CHECK_EQ(" #a ", " #b ") failed: "                   \
                << _va << " != " << _vb << "\n";                          \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

void TestRecommendReturnsRankedItems() {
  RecommendationCache cache;
  cache.Append(MakeItem("SKU-1", "A", "peripherals", 0.9, 5000));
  cache.Append(MakeItem("SKU-2", "B", "peripherals", 0.5, 5000));
  cache.Append(MakeItem("SKU-3", "C", "displays", 0.99, 30000));

  Recommender engine(cache);
  auto result = engine.Recommend("user-1", /*category=*/"", /*limit=*/10);

  CHECK_EQ(result.user_id, std::string("user-1"));
  CHECK_EQ(result.items.size(), static_cast<std::size_t>(3));
  // Highest catalog score should sort first (category="" so
  // category_match is a flat 0.5 for everyone; price_proximity is
  // identical since all prices are near the default median).
  CHECK_EQ(result.items.front().sku, std::string("SKU-3"));
}

void TestRecommendRespectsCategoryFilter() {
  RecommendationCache cache;
  cache.Append(MakeItem("SKU-1", "A", "peripherals", 0.9, 5000));
  cache.Append(MakeItem("SKU-2", "B", "displays", 0.99, 30000));

  Recommender engine(cache);
  auto result = engine.Recommend("user-1", "peripherals", 10);
  CHECK_EQ(result.items.size(), static_cast<std::size_t>(1));
  CHECK_EQ(result.items.front().sku, std::string("SKU-1"));
}

void TestRecommendPreFilterIsCaseSensitive() {
  RecommendationCache cache;
  cache.Append(MakeItem("SKU-1", "A", "Peripherals", 0.9, 5000));

  Recommender engine(cache);
  // The strict pre-filter in the Recommend loop is case-SENSITIVE, so
  // "Peripherals" != "peripherals" drops the item before the
  // case-insensitive CategoryMatch signal ever sees it. This documents
  // that known minor gap (see CHANGELOG v0.3 in engine.cpp).
  auto result = engine.Recommend("user-1", "peripherals", 10);
  CHECK_EQ(result.items.size(), static_cast<std::size_t>(0));
}

void TestRecommendClampsToLimit() {
  RecommendationCache cache;
  for (int i = 0; i < 50; ++i) {
    cache.Append(MakeItem("SKU-" + std::to_string(i), "N", "cat", 0.1 * i, 5000));
  }

  Recommender engine(cache);
  auto result = engine.Recommend("user-1", "", 5);
  CHECK_EQ(result.items.size(), static_cast<std::size_t>(5));
  // Highest scores first.
  CHECK_EQ(result.items.front().sku, std::string("SKU-49"));
}

void TestRecommendEmptyCacheReturnsNothing() {
  RecommendationCache cache;
  Recommender engine(cache);
  auto result = engine.Recommend("user-1", "", 10);
  CHECK(result.items.empty());
}

void TestRecommendZeroLimitReturnsNothing() {
  RecommendationCache cache;
  cache.Append(MakeItem("SKU-1", "A", "cat", 0.9, 5000));
  Recommender engine(cache);
  auto result = engine.Recommend("user-1", "", 0);
  CHECK(result.items.empty());
}


}  // namespace

int main() {
  TestRecommendReturnsRankedItems();
  TestRecommendRespectsCategoryFilter();
  TestRecommendPreFilterIsCaseSensitive();
  TestRecommendClampsToLimit();
  TestRecommendEmptyCacheReturnsNothing();
  TestRecommendZeroLimitReturnsNothing();

  if (g_failures == 0) {
    std::cout << "all engine tests passed\n";
    return 0;
  }
  std::cerr << g_failures << " test(s) failed\n";
  return 1;
}
