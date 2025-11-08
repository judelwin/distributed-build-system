# Multi-stage Dockerfile for building C++ distributed build system
FROM ubuntu:22.04 AS builder

# Install build dependencies including gRPC and protobuf from apt
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libhiredis-dev \
    libopenssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy project files
COPY CMakeLists.txt vcpkg.json ./
COPY proto/ ./proto/
COPY src/ ./src/

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libgrpc++1 \
    libprotobuf23 \
    libhiredis0.14 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy binaries from builder
WORKDIR /app
COPY --from=builder /app/build/bin/coordinator /app/build/bin/worker ./

# Create directories for logs and benchmarks
RUN mkdir -p /app/logs /app/benchmarks

# Default command (can be overridden in docker-compose)
CMD ["./coordinator"]

