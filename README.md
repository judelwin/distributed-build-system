
# Distributed Build System

This project implements a distributed build system for C++ using gRPC and Redis. The goal is to parallelize compilation tasks across multiple worker nodes, improving build efficiency and scalability.

## Features
- Coordinator service parses a dependency graph and schedules build tasks
- Worker nodes receive tasks via gRPC and perform compilation
- Redis is used to cache build artifacts and metadata for faster incremental builds
- Fault tolerance with automatic retry and worker failure detection
- Docker Compose is provided for local development and testing with multiple workers

## Getting Started
To build and run the system locally:

```bash
docker compose up --build
```

## Benchmarking
- Supports clusters of 1, 3, or 5 workers
- Outputs build metrics and cache statistics to CSV files in the `benchmarks/` directory

## Project Structure

### Core Components
- `src/common/` — Shared types and utilities
  - Task structures and dependency graph parsing
  - Hash computation utilities
  - JSON dependency graph parsing

- `src/coordinator/` — Build task coordinator service
  - Task scheduling and dependency resolution
  - gRPC client pool for worker communication
  - Redis caching for build artifacts
  - Fault tolerance and retry logic

- `src/worker/` — Build task worker service
  - gRPC service implementation for task execution
  - Compilation logic
  - Redis integration for artifact storage

### Supporting Files
- `proto/` — Protocol Buffers definitions for gRPC services
- `benchmarks/` — Scripts and results for performance evaluation
- `Dockerfile`, `docker-compose.yml` — Containerization and orchestration

## Requirements
- C++17 or newer
- gRPC and Protocol Buffers
- Redis
- Docker

## License
This project is released under the MIT License.
