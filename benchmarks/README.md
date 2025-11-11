# Benchmarking

This directory contains scripts and results for benchmarking the distributed build system.

## Running Benchmarks

To run the full benchmark suite (1, 3, and 5 workers):

```bash
./benchmarks/run_benchmarks.sh [path_to_build_graph.json]
```

If no build graph is specified, it will use `example_build_graph.json` from the project root.

## Output

Benchmark results are saved to `benchmarks/results/`:
- `benchmark_1workers.csv` - Results for 1 worker configuration
- `benchmark_3workers.csv` - Results for 3 workers configuration
- `benchmark_5workers.csv` - Results for 5 workers configuration
- `summary.txt` - Aggregated summary report

## Metrics Collected

Each CSV file contains the following metrics:
- `num_workers` - Number of workers in the cluster
- `total_tasks` - Total number of tasks in the build graph
- `completed_tasks` - Number of successfully completed tasks
- `cached_tasks` - Number of tasks served from cache
- `failed_tasks` - Number of failed tasks
- `retried_tasks` - Number of tasks that were retried
- `total_build_time_ms` - Total build time in milliseconds
- `min_task_time_ms` - Minimum task execution time
- `max_task_time_ms` - Maximum task execution time
- `avg_task_time_ms` - Average task execution time
- `tasks_per_second` - Throughput metric
- `cache_hit_rate` - Cache hit rate percentage

## Manual Benchmarking

You can also run individual benchmarks using the coordinator directly:

```bash
# With 1 worker
docker compose up -d
docker compose exec coordinator /app/bin/coordinator \
    --json /app/example_build_graph.json \
    --workers worker-1:50052 \
    --csv benchmarks/results/manual_1worker.csv

# With 3 workers
docker compose --profile 3-workers up -d
docker compose exec coordinator /app/bin/coordinator \
    --json /app/example_build_graph.json \
    --workers worker-1:50052,worker-2:50053,worker-3:50054 \
    --csv benchmarks/results/manual_3workers.csv
```

## Generating Summary

To generate a summary report from existing CSV files:

```bash
python3 benchmarks/generate_summary.py benchmarks/results
```

