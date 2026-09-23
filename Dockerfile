# syntax=docker/dockerfile:1.6

# ---- Build stage ----
FROM gcc:13-bookworm AS builder

# Build tools + gRPC/Protobuf development libraries. We use the Debian
# packages rather than building gRPC from source to keep the image
# build under ~10 minutes.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        pkg-config \
        libgrpc++-dev \
        libprotobuf-dev \
        protobuf-compiler \
        protobuf-compiler-grpc \
        libabsl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY CMakeLists.txt ./
COPY src/ ./src/
COPY tests/ ./tests/

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel "$(nproc)"

# ---- Runtime stage ----
FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libgrpc++1.51 \
        libprotobuf32 \
        libabsl20220623 \
        libatomic1 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd -r app && useradd -r -g app -d /app app

WORKDIR /app

COPY --from=builder /build/build/recommendation-engine /usr/local/bin/recommendation-engine

# The binary is compiled with gcc 13 and requires its runtime libraries
# (GLIBCXX_3.4.32); bookworm's libstdc++6 only ships 3.4.30. Copy the
# compiler's lib dir from the build stage and let the loader find it
# first. Newer libstdc++ is backwards-compatible with the bookworm
# libgrpc++/libabsl the binary also links.
COPY --from=builder /usr/local/lib64 /usr/local/lib64
ENV LD_LIBRARY_PATH=/usr/local/lib64

USER app

EXPOSE 50051

ENV BIND=0.0.0.0:50051

CMD ["recommendation-engine"]
