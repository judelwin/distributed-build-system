#!/usr/bin/env python3
"""
Generate a summary report from benchmark CSV files.
"""

import sys
import os
import csv
from pathlib import Path
from datetime import datetime

def read_csv(filename):
    """Read a CSV file and return the data as a list of dictionaries."""
    if not os.path.exists(filename):
        return []
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        return list(reader)

def generate_summary(results_dir):
    """Generate a summary report from benchmark results."""
    results_path = Path(results_dir)
    
    summary_file = results_path / "summary.txt"
    
    with open(summary_file, 'w') as f:
        f.write("Benchmark Summary\n")
        f.write("=" * 50 + "\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        # Read results for each worker configuration
        for num_workers in [1, 3, 5]:
            csv_file = results_path / f"benchmark_{num_workers}workers.csv"
            
            if not csv_file.exists():
                f.write(f"\n{num_workers} Worker(s): No data available\n")
                continue
            
            data = read_csv(csv_file)
            if not data:
                f.write(f"\n{num_workers} Worker(s): Empty results\n")
                continue
            
            # Use first row (assuming single run per CSV)
            row = data[0]
            
            f.write(f"\n{num_workers} Worker(s) Configuration\n")
            f.write("-" * 50 + "\n")
            f.write(f"Total Tasks: {row.get('total_tasks', 'N/A')}\n")
            f.write(f"Completed Tasks: {row.get('completed_tasks', 'N/A')}\n")
            f.write(f"Cached Tasks: {row.get('cached_tasks', 'N/A')}\n")
            f.write(f"Failed Tasks: {row.get('failed_tasks', 'N/A')}\n")
            f.write(f"Retried Tasks: {row.get('retried_tasks', 'N/A')}\n")
            f.write(f"\nTiming Metrics:\n")
            f.write(f"  Total Build Time: {row.get('total_build_time_ms', 'N/A')} ms\n")
            f.write(f"  Min Task Time: {row.get('min_task_time_ms', 'N/A')} ms\n")
            f.write(f"  Max Task Time: {row.get('max_task_time_ms', 'N/A')} ms\n")
            f.write(f"  Avg Task Time: {row.get('avg_task_time_ms', 'N/A')} ms\n")
            f.write(f"\nThroughput:\n")
            f.write(f"  Tasks per Second: {row.get('tasks_per_second', 'N/A')}\n")
            f.write(f"\nCache Performance:\n")
            f.write(f"  Cache Hit Rate: {row.get('cache_hit_rate', 'N/A')}%\n")
            f.write("\n")
        
        # Comparison section
        f.write("\n" + "=" * 50 + "\n")
        f.write("Performance Comparison\n")
        f.write("=" * 50 + "\n\n")
        
        results = {}
        for num_workers in [1, 3, 5]:
            csv_file = results_path / f"benchmark_{num_workers}workers.csv"
            if csv_file.exists():
                data = read_csv(csv_file)
                if data:
                    results[num_workers] = data[0]
        
        if len(results) > 1:
            f.write(f"{'Workers':<10} {'Build Time (ms)':<20} {'Throughput (tasks/s)':<25} {'Cache Hit %':<15}\n")
            f.write("-" * 70 + "\n")
            for num_workers in sorted(results.keys()):
                row = results[num_workers]
                f.write(f"{num_workers:<10} "
                       f"{row.get('total_build_time_ms', 'N/A'):<20} "
                       f"{row.get('tasks_per_second', 'N/A'):<25} "
                       f"{row.get('cache_hit_rate', 'N/A'):<15}\n")
    
    print(f"Summary generated: {summary_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: generate_summary.py <results_directory>")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    generate_summary(results_dir)

