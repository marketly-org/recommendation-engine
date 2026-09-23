// server.hpp — gRPC service implementation for the recommendation engine.
#pragma once

#include <grpcpp/grpcpp.h>

#include "cache.hpp"
#include "engine.hpp"
#include "recommendation.grpc.pb.h"
#include "recommendation.pb.h"

namespace marketly::recommendation {

// Alias for the protobuf-generated namespace (proto package
// `marketly.recommendation.v1`). Keeps proto type references short
// without colliding with the domain RecommendationItem struct.
namespace proto = ::marketly::recommendation::v1;

// gRPC service implementation. Routes RPCs to the engine + cache.
//
// GetRecommendations   -> Recommender::Recommend (reads the cache)
// AppendItem           -> RecommendationCache::Append (mutates the cache)
// Health               -> returns pool size + version
//
// The read and write paths run on different gRPC threads against the
// same process-wide cache; see engine.cpp / cache.hpp for the
// concurrency bug.
class RecommendationServiceImpl final
    : public proto::RecommendationService::Service {
 public:
  RecommendationServiceImpl(RecommendationCache& cache, Recommender& engine);

  grpc::Status GetRecommendations(grpc::ServerContext* context,
                                  const proto::RecommendRequest* request,
                                  proto::RecommendResponse* response) override;

  grpc::Status AppendItem(grpc::ServerContext* context,
                          const proto::AppendItemRequest* request,
                          proto::AppendItemResponse* response) override;

  grpc::Status Health(grpc::ServerContext* context,
                      const proto::HealthRequest* request,
                      proto::HealthResponse* response) override;

 private:
  RecommendationCache& cache_;
  Recommender& engine_;
};

}  // namespace marketly::recommendation
