
# Distributed Build System

This project implements a distributed build system for C++ using gRPC and Redis. The goal is to parallelize compilation tasks across multiple worker nodes, improving build efficiency and scalability.

## Features
- Coordinator service parses a dependency graph and schedules build tasks
- Worker nodes receive tasks via gRPC and perform compilation
- Redis is used to cache build artifacts and metadata for faster incremental builds
- Fault tolerance with automatic retry and worker failure detection
- Performance metrics collection and benchmarking tools
- Docker Compose is provided for local development and testing with multiple workers

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Coordinator                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  Scheduler   │  │ gRPC Client  │  │   Redis Client      │  │
│  │ (Topological │  │     Pool     │  │   (Cache Lookup)    │  │
│  │     Sort)    │  │              │  │                      │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │   Fault      │  │   Metrics    │  │  Dependency Graph    │  │
│  │  Tolerance   │  │  Collector   │  │      Parser          │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ gRPC (SubmitTask, GetStatus, GetResult)
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Worker 1   │    │   Worker 2  │    │   Worker N   │
│              │    │              │    │              │
│ ┌──────────┐ │    │ ┌──────────┐ │    │ ┌──────────┐ │
│ │  gRPC    │ │    │ │  gRPC    │ │    │ │  gRPC    │ │
│ │  Server  │ │    │ │  Server  │ │    │ │  Server  │ │
│ └──────────┘ │    │ └──────────┘ │    │ └──────────┘ │
│ ┌──────────┐ │    │ ┌──────────┐ │    │ ┌──────────┐ │
│ │ Compiler │ │    │ │ Compiler │ │    │ │ Compiler │ │
│ └──────────┘ │    │ └──────────┘ │    │ └──────────┘ │
│ ┌──────────┐ │    │ ┌──────────┐ │    │ ┌──────────┐ │
│ │  Redis   │ │    │ │  Redis   │ │    │ │  Redis   │ │
│ │  Client  │ │    │ │  Client  │ │    │ │  Client  │ │
│ └──────────┘ │    │ └──────────┘ │    │ └──────────┘ │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │     Redis     │
                    │   (Cache)     │
                    │               │
                    │  Artifact     │
                    │  Metadata     │
                    └──────────────┘
```

### Workflow

1. **Dependency Graph Parsing**: Coordinator reads a JSON file describing build tasks and their dependencies
2. **Topological Scheduling**: Tasks are ordered using topological sort to respect dependencies
3. **Cache Lookup**: Before scheduling, coordinator checks Redis cache using content hash
4. **Task Dispatch**: Ready tasks are distributed to available workers via gRPC
5. **Compilation**: Workers execute compilation tasks and return results
6. **Artifact Storage**: Successful builds store artifacts in Redis for future cache hits
7. **Fault Handling**: Failed tasks are automatically retried on different workers

## Getting Started

### Prerequisites
- Docker and Docker Compose
- C++17 compatible compiler (for local development)

### Running the System

Build and start all services:

```bash
docker compose up --build
```

Run with specific worker configurations:

```bash
# 1 worker (default)
docker compose up

# 3 workers
docker compose --profile 3-workers up

# 5 workers
docker compose --profile 5-workers up
```

### Running a Build

The coordinator accepts a JSON build graph file. Example usage:

```bash
docker compose exec coordinator /app/bin/coordinator \
    --json /app/example_build_graph.json \
    --workers worker-1:50052,worker-2:50053,worker-3:50054
```

## Benchmarking

The system includes comprehensive benchmarking tools to measure performance across different worker configurations.

### Running Benchmarks

```bash
./benchmarks/run_benchmarks.sh [path_to_build_graph.json]
```

This will run experiments with 1, 3, and 5 workers and generate CSV results in `benchmarks/results/`.

### Metrics Collected

- **Task Metrics**: Total, completed, cached, failed, retried tasks
- **Timing**: Total build time, min/max/avg task execution time
- **Throughput**: Tasks per second
- **Cache Performance**: Cache hit rate percentage

See `benchmarks/README.md` for detailed benchmarking documentation.

## Results

Benchmark results demonstrate the system's scalability and caching effectiveness:

- **Scalability**: Build time decreases as worker count increases (1 → 3 → 5 workers)
- **Cache Performance**: Subsequent builds show significant speedup due to artifact caching
- **Fault Tolerance**: System handles worker failures gracefully with automatic retry

Detailed results are available in `benchmarks/results/` after running the benchmark suite.

## Project Structure

### Core Components

- `src/common/` — Shared types and utilities
  - `build_task.h/cpp` — Task structures and execution tracking
  - `dependency_graph.h/cpp` — JSON parsing and topological sort
  - `hash_utils.h/cpp` — SHA256 hashing for content-based caching

- `src/coordinator/` — Build task coordinator service
  - `main.cpp` — Main orchestration logic
  - `scheduler.h/cpp` — Dependency-aware task scheduling
  - `grpc_client_pool.h/cpp` — Worker connection management
  - `redis_client.h/cpp` — Cache lookup and storage
  - `fault_tolerance.h/cpp` — Retry logic and failure handling
  - `metrics.h/cpp` — Performance metrics collection

- `src/worker/` — Build task worker service
  - `main.cpp` — gRPC server setup
  - `build_service_impl.h/cpp` — gRPC service implementation
  - `compiler.h/cpp` — Compilation execution
  - `redis_client.h/cpp` — Artifact storage

### Supporting Files

- `proto/` — Protocol Buffers definitions for gRPC services
- `benchmarks/` — Scripts and results for performance evaluation
  - `run_benchmarks.sh` — Automated benchmark runner
  - `generate_summary.py` — Results aggregation script
- `Dockerfile` — Multi-stage build for optimized container images
- `docker-compose.yml` — Service orchestration with profiles for different worker counts

## Requirements

- **C++17** or newer
- **gRPC** and **Protocol Buffers** for inter-service communication
- **Redis** for artifact caching
- **Docker** and **Docker Compose** for containerization
- **CMake** 3.20+ for building
