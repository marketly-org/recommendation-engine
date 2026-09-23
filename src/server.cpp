// server.cpp — gRPC service implementation.
#include "server.hpp"

#include <grpcpp/grpcpp.h>
#include <string>

#include "cache.hpp"
#include "engine.hpp"
#include "models.hpp"
#include "recommendation.grpc.pb.h"
#include "recommendation.pb.h"

namespace marketly::recommendation {

namespace {

constexpr const char* kServiceVersion = "0.3.0";

RecommendationItem FromProto(const proto::RecommendationItem& p) {
  RecommendationItem item;
  item.sku = p.sku();
  item.name = p.name();
  item.category = p.category();
  item.score = p.score();
  item.price_cents = p.price_cents();
  return item;
}

proto::RecommendationItem ToProto(const RecommendationItem& item) {
  proto::RecommendationItem p;
  p.set_sku(item.sku);
  p.set_name(item.name);
  p.set_category(item.category);
  p.set_score(item.score);
  p.set_price_cents(item.price_cents);
  return p;
}

}  // namespace

RecommendationServiceImpl::RecommendationServiceImpl(
    RecommendationCache& cache, Recommender& engine)
    : cache_(cache), engine_(engine) {}

grpc::Status RecommendationServiceImpl::GetRecommendations(
    grpc::ServerContext* context,
    const proto::RecommendRequest* request,
    proto::RecommendResponse* response) {
  if (request->user_id().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "user_id is required");
  }

  const std::int32_t limit = request->limit() > 0 ? request->limit() : 10;

  // This calls into engine.cpp:Recommend (the buggy iteration).
  RecommendationResult result =
      engine_.Recommend(request->user_id(), request->category(), limit);

  response->set_user_id(result.user_id);
  auto it = context->client_metadata().find("x-trace-id");
  if (it != context->client_metadata().end()) {
    response->set_trace_id(std::string(it->second.data(), it->second.size()));
  }
  for (const auto& item : result.items) {
    *response->add_items() = ToProto(item);
  }
  return grpc::Status::OK;
}

grpc::Status RecommendationServiceImpl::AppendItem(
    grpc::ServerContext* /*context*/,
    const proto::AppendItemRequest* request,
    proto::AppendItemResponse* response) {
  if (request->item().sku().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "item.sku is required");
  }

  // Mutates the shared cache. Under a concurrent GetRecommendations
  // call this is what invalidates the engine's iterator (see engine.cpp).
  cache_.Append(FromProto(request->item()));

  response->set_accepted(true);
  response->set_pool_size(static_cast<std::int32_t>(cache_.Size()));
  return grpc::Status::OK;
}

grpc::Status RecommendationServiceImpl::Health(
    grpc::ServerContext* /*context*/,
    const proto::HealthRequest* /*request*/,
    proto::HealthResponse* response) {
  response->set_status("ok");
  response->set_service("recommendation-engine");
  response->set_version(kServiceVersion);
  response->set_pool_size(static_cast<std::int32_t>(cache_.Size()));
  return grpc::Status::OK;
}

}  // namespace marketly::recommendation
