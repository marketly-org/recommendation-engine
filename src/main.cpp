// main.cpp — bootstrap the recommendation-engine gRPC server.
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "cache.hpp"
#include "engine.hpp"
#include "server.hpp"

namespace {

constexpr const char* kDefaultBind = "0.0.0.0:50051";

std::string bind_address() {
  const char* env = std::getenv("PORT");
  if (env && std::string(env) == "50051") return kDefaultBind;
  const char* addr = std::getenv("BIND");
  return (addr && *addr) ? std::string(addr) : std::string(kDefaultBind);
}

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
  using namespace marketly::recommendation;

  // Enable the built-in gRPC Health Checking Protocol service so k8s
  // grpcProbe (and Envoy) can liveness-check us on port 50051.
  grpc::EnableDefaultHealthCheckService(true);

  auto& cache = GlobalCache();
  static Recommender engine(cache);
  RecommendationServiceImpl service(cache, engine);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(bind_address(), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  if (!server) {
    std::cerr << "recommendation-engine: failed to bind on "
              << bind_address() << "\n";
    return 1;
  }

  std::cout << "recommendation-engine listening on " << bind_address()
            << " (pool_size=" << cache.Size() << ")\n";
  server->Wait();
  return 0;
}
