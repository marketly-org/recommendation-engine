// cache.cpp — implementation of the recommendation candidate pool.
#include "cache.hpp"

namespace marketly::recommendation {

void RecommendationCache::Append(RecommendationItem item) {
  // BUG: no lock. A concurrent reader iterating `items_` (see
  // engine.cpp `recommend()`) will dereference an invalidated
  // iterator when this push_back triggers a reallocation.
  items_.push_back(std::move(item));
}

void RecommendationCache::Clear() { items_.clear(); }

RecommendationCache& GlobalCache() {
  static RecommendationCache instance;
  return instance;
}

}  // namespace marketly::recommendation
