# recommendation-engine

gRPC recommendation service for the **Marketly** e-commerce platform.

Returns ranked product recommendations for a given user. The candidate
pool is an in-memory `std::vector` mutated by the `AppendItem` RPC
(catalog ingestion) and read by `GetRecommendations`. Ranking uses a
three-signal blend: catalog relevance, category match, and price
proximity (see `src/engine.cpp` for the full scoring model).

## Stack

- **C++17** + **gRPC** + **Protobuf**
- CMake build, Ninja generator

## gRPC API

| Method | Request | Response | Description |
|--------|---------|----------|-------------|
| `GetRecommendations` | `RecommendRequest` | `RecommendResponse` | Top-N recommendations |
| `AppendItem` | `AppendItemRequest` | `AppendItemResponse` | Add a candidate to the pool |
| `Health` | `HealthRequest` | `HealthResponse` | Custom health (pool size) |

The standard gRPC Health Checking Protocol (`grpc.health.v1.Health`) is
also served (via `EnableDefaultHealthCheckService`) for k8s
`grpcProbe`.

## Local development

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/recommendation-engine          # listens on 0.0.0.0:50051
```

## Tests

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

## Configuration

| Env var | Default | Description |
|---------|---------|-------------|
| `BIND` | `0.0.0.0:50051` | gRPC listen address |
