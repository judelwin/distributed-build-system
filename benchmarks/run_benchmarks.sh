#!/bin/bash

# Benchmark script for distributed build system
# Runs experiments with 1, 3, and 5 workers

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BENCHMARK_DIR="$PROJECT_ROOT/benchmarks"
RESULTS_DIR="$BENCHMARK_DIR/results"
JSON_FILE="${1:-$PROJECT_ROOT/example_build_graph.json}"

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "Starting benchmark suite..."
echo "Using build graph: $JSON_FILE"
echo "Results will be saved to: $RESULTS_DIR"
echo ""

# Copy JSON file to container-accessible location
cp "$JSON_FILE" "$BENCHMARK_DIR/build_graph.json" 2>/dev/null || true

# Function to run a benchmark with N workers
run_benchmark() {
    local num_workers=$1
    local profile_flag=""
    
    echo "========================================="
    echo "Running benchmark with $num_workers worker(s)..."
    echo "========================================="
    
    # Determine Docker Compose profile
    case $num_workers in
        1) profile_flag="" ;;
        3) profile_flag="--profile 3-workers" ;;
        5) profile_flag="--profile 5-workers" ;;
        *) echo "Invalid worker count: $num_workers"; return 1 ;;
    esac
    
    # Start services with appropriate profile
    echo "Starting services..."
    cd "$PROJECT_ROOT"
    docker compose $profile_flag up -d --build
    
    # Wait for services to be ready
    echo "Waiting for services to be ready..."
    sleep 5
    
    # Build worker addresses based on count
    local worker_addresses=""
    for i in $(seq 1 $num_workers); do
        if [ $i -gt 1 ]; then
            worker_addresses="${worker_addresses},"
        fi
        worker_addresses="${worker_addresses}worker-${i}:5005$((1 + i))"
    done
    
    # Copy JSON file into coordinator container
    docker cp "$JSON_FILE" coordinator:/app/build_graph.json 2>/dev/null || \
    docker cp "$BENCHMARK_DIR/build_graph.json" coordinator:/app/build_graph.json 2>/dev/null || true
    
    # Run coordinator with CSV output
    local output_file="benchmarks/results/benchmark_${num_workers}workers.csv"
    docker compose exec -T coordinator /app/bin/coordinator \
        --json /app/build_graph.json \
        --workers "$worker_addresses" \
        --csv "$output_file" || {
        echo "Warning: Benchmark failed for $num_workers worker(s)"
        docker compose $profile_flag down
        return 1
    }
    
    echo "Results saved to: $output_file"
    
    # Stop services
    docker compose $profile_flag down
    echo ""
}

# Run benchmarks for 1, 3, and 5 workers
echo "Starting benchmarks..."
echo ""

# 1 worker
run_benchmark 1

# 3 workers
run_benchmark 3

# 5 workers
run_benchmark 5

# Generate summary using Python script if available
if command -v python3 &> /dev/null && [ -f "$BENCHMARK_DIR/generate_summary.py" ]; then
    echo "========================================="
    echo "Generating summary..."
    echo "========================================="
    python3 "$BENCHMARK_DIR/generate_summary.py" "$RESULTS_DIR"
else
    # Fallback to simple text summary
    SUMMARY_FILE="$RESULTS_DIR/summary.txt"
    cat > "$SUMMARY_FILE" << EOF
Benchmark Summary
=================
Generated: $(date)

Configuration:
- Build graph: $JSON_FILE
- Workers tested: 1, 3, 5

Results:
EOF

    for workers in 1 3 5; do
        csv_file="$RESULTS_DIR/benchmark_${workers}workers.csv"
        if [ -f "$csv_file" ]; then
            echo "" >> "$SUMMARY_FILE"
            echo "--- $workers Worker(s) ---" >> "$SUMMARY_FILE"
            cat "$csv_file" >> "$SUMMARY_FILE"
        fi
    done
    
    echo "Summary saved to: $SUMMARY_FILE"
fi

echo ""
echo "Benchmark suite completed!"

