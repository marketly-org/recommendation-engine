// test_engine.cpp — unit tests for the recommendation engine.
//
// These tests run single-threaded and so do NOT exercise the
// iterator-invalidation concurrency bug (see engine.cpp / cache.hpp).
// They cover the ranking math + filtering + limit clamping.
//
// Sentinel should fix the concurrency bug and then ADD a test that
// runs the engine under concurrent Append() load — see the disabled
// test at the bottom.

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

/*
 * DISABLED — Sentinel should ENABLE this after fixing the bug.
 *
 * This test spawns a writer thread that Appends to the cache while the
 * main thread calls Recommend in a tight loop. With the current (buggy)
 * unsynchronised cache, the engine's iterator is invalidated mid-loop
 * and the process segfaults.
 *
 * void TestRecommendUnderConcurrentAppend() {
 *   RecommendationCache cache;
 *   for (int i = 0; i < 1000; ++i)
 *     cache.Append(MakeItem("seed-" + std::to_string(i), "n", "c", 0.5, 5000));
 *   Recommender engine(cache);
 *   std::atomic<bool> stop{false};
 *   std::thread writer([&]{
 *     for (int i = 0; !stop; ++i)
 *       cache.Append(MakeItem("w-" + std::to_string(i), "n", "c", 0.5, 5000));
 *   });
 *   for (int i = 0; i < 10000; ++i)
 *     engine.Recommend("u", "", 10);   // would segfault today
 *   stop = true;
 *   writer.join();
 * }
 */

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
