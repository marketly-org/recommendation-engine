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
//
// ------------------------------------------------------------------
// BUG: the pool is backed by a plain std::vector<RecommendationItem>
// with NO synchronization. `append()` mutates the vector on one
// thread while `items()` is being iterated on another. std::vector
// invalidates all iterators (including end()) when it reallocates
// during a push_back, so the iterating thread dereferences a dangling
// iterator and segfaults:
//
//   Segmentation fault (core dumped)
//   #0 std::vector<RecommendationItem>::begin()
//   #1 recommend(...) at src/engine.cpp:142
//   #2 RecommendationServiceImpl::GetRecommendations(...)
//
// FIX: guard the vector with a std::shared_mutex.
//   * append() takes an exclusive lock (std::unique_lock).
//   * items() / iteration takes a shared lock (std::shared_lock).
// Better still, expose a `snapshot()` that returns a copy of the
// vector under a shared lock, so the engine iterates its own private
// copy and never sees a mid-iteration reallocation.
// ------------------------------------------------------------------
class RecommendationCache {
 public:
  RecommendationCache() = default;

  // Append a candidate to the pool. NOT thread-safe vs `items()`.
  void Append(RecommendationItem item);

  // Read-only access to the underlying vector. Callers MUST NOT
  // mutate the returned reference, and MUST NOT hold it across an
  // `Append()` call on another thread (see bug note above).
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
