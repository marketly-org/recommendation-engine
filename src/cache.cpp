// cache.cpp — implementation of the recommendation candidate pool.
#include "cache.hpp"
#include <mutex>

namespace marketly::recommendation {

static std::mutex g_cache_mutex;

void RecommendationCache::Append(RecommendationItem item) {
  std::lock_guard<std::mutex> lock(g_cache_mutex);
  items_.push_back(std::move(item));
}

void RecommendationCache::Clear() {
  std::lock_guard<std::mutex> lock(g_cache_mutex);
  items_.clear();
}

RecommendationCache& GlobalCache() {
  static RecommendationCache instance;
  return instance;
}

}  // namespace marketly::recommendation
