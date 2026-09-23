// cache.hpp — in-memory pool of recommendation candidates.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "models.hpp"

namespace marketly::recommendation {

// In-memory pool of recommendation candidates.
//
// The pool is mutated by the AppendItem RPC (catalog-ingestion
// pipeline) and read by the GetRecommendations RPC (recommendation
// engine). Both run on different gRPC threads against the same
// process-wide pool.
class RecommendationCache {
 public:
  RecommendationCache() = default;

  // Append a candidate to the pool.
  void Append(RecommendationItem item);

  // Read-only access to the underlying vector. Callers MUST NOT
  // mutate the returned reference.
  const std::vector<RecommendationItem>& Items() const { return items_; }

  // Number of candidates currently in the pool.
  std::size_t Size() const { return items_.size(); }

  // Clear the pool. Used by integration tests.
  void Clear();

 private:
  std::vector<RecommendationItem> items_;
};

// Process-wide singleton cache. The gRPC server and the engine share
// this instance.
RecommendationCache& GlobalCache();

}  // namespace marketly::recommendation
