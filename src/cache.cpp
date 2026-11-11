// cache.cpp — implementation of the recommendation candidate pool.
#include "cache.hpp"

namespace marketly::recommendation {

void RecommendationCache::Append(RecommendationItem item) {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.push_back(std::move(item));
}

void RecommendationCache::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}

RecommendationCache& GlobalCache() {
  static RecommendationCache instance;
  return instance;
}

}  // namespace marketly::recommendation
