// models.hpp — domain types shared across the recommendation engine.
#pragma once

#include <cstdint>
#include <string>

namespace marketly::recommendation {

// A single recommendation candidate.
//
// This mirrors the protobuf `RecommendationItem` message but is a plain
// POD struct so the cache and engine can operate on it without pulling
// in the generated protobuf headers.
struct RecommendationItem {
  std::string sku;
  std::string name;
  std::string category;
  double score = 0.0;          // relevance score in [0, 1]
  std::int32_t price_cents = 0;
};

}  // namespace marketly::recommendation
