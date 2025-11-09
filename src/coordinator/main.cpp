#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sstream>

#include "scheduler.h"
#include "grpc_client_pool.h"
#include "redis_client.h"
#include "fault_tolerance.h"
#include "build_task.h"
#include "dependency_graph.h"
#include "build.grpc.pb.h"

using build::Task;
using build::TaskStatus;
using build::TaskRequest;
using build::TaskResponse;
using build::StatusResponse;
using build::ResultResponse;

// Coordinator class using modular components
class Coordinator {
public:
    Coordinator(const std::vector<std::string>& worker_addresses,
                const std::string& redis_host, int redis_port)
        : client_pool_(worker_addresses),
          redis_(redis_host, redis_port),
          fault_tolerance_(3) {}

    // Schedule and execute build tasks
    void execute_build(const std::string& json_file) {
        auto tasks = build::parse_dependency_graph(json_file);
        
        std::cout << "Parsed " << tasks.size() << " tasks" << std::endl;

        scheduler_.initialize(tasks);
        std::unordered_map<std::string, TaskStatus> task_status;
        std::unordered_map<std::string, int> task_to_worker;
        
        int completed = 0;
        int cached = 0;
        int retried = 0;
        int failed = 0;

        // Process ready tasks (can execute in parallel)
        while (!scheduler_.all_completed(scheduler_.get_total_tasks())) {
            // Get ready tasks
            auto tasks_to_process = scheduler_.get_ready_tasks(client_pool_.size());

            // Process each ready task
            for (const auto& task_id : tasks_to_process) {
                auto& task = tasks[task_id];
                
                // Skip if already completed
                if (scheduler_.is_completed(task_id)) {
                    continue;
                }

                // Check cache before scheduling (idempotency via content hash)
                if (redis_.is_available() && redis_.exists(task.content_hash)) {
                    std::cout << "Task " << task_id << " found in cache (hash: " 
                              << task.content_hash.substr(0, 8) << ")" << std::endl;
                    scheduler_.mark_completed(task_id);
                    cached++;
                    completed++;
                    continue;
                }

                // Check if task needs retry
                bool needs_retry = fault_tolerance_.needs_retry(task_id);
                int retry_worker_idx = fault_tolerance_.get_failed_worker(task_id);

                // Select worker (avoid failed worker on retry)
                int worker_idx;
                if (needs_retry) {
                    worker_idx = client_pool_.select_worker();
                    if (worker_idx == retry_worker_idx && client_pool_.size() > 1) {
                        worker_idx = (worker_idx + 1) % client_pool_.size();
                    }
                    std::cout << "Retrying task " << task_id << " on worker " 
                              << worker_idx << " (attempt " 
                              << fault_tolerance_.get_retry_count(task_id) + 1 << ")" << std::endl;
                    retried++;
                } else {
                    worker_idx = client_pool_.select_worker();
                }

                auto* worker = client_pool_.get_client(worker_idx);
                if (!worker) {
                    std::cerr << "Invalid worker index: " << worker_idx << std::endl;
                    continue;
                }

                // Create task request
                TaskRequest request;
                request.set_task_id(task.id);
                request.set_content_hash(task.content_hash);
                for (const auto& file : task.source_files) {
                    request.add_source_files(file);
                }
                for (const auto& dep : task.dependencies) {
                    request.add_dependencies(dep);
                }
                for (const auto& flag : task.compile_flags) {
                    request.add_compile_flags(flag);
                }
                request.set_output_path(task.output_path);
                request.set_timeout_seconds(task.timeout_seconds);

                // Submit task to worker
                TaskResponse response;
                bool submit_success = false;
                try {
                    submit_success = worker->SubmitTask(request, response);
                } catch (const std::exception& e) {
                    std::cerr << "Exception submitting task " << task_id 
                              << " to worker " << worker_idx << ": " << e.what() << std::endl;
                    submit_success = false;
                }

                if (submit_success && response.accepted()) {
                    std::cout << "Submitted task " << task_id << " to worker " 
                              << worker_idx << std::endl;
                    
                    fault_tolerance_.record_start(task_id, worker_idx, 
                                                  task.timeout_seconds, 
                                                  task.content_hash, needs_retry);
                    task_status[task_id] = TaskStatus::IN_PROGRESS;
                    task_to_worker[task_id] = worker_idx;
                } else {
                    std::cerr << "Failed to submit task " << task_id 
                              << " to worker " << worker_idx << std::endl;
                    fault_tolerance_.record_failure(task_id);
                    
                    if (fault_tolerance_.exceeded_max_retries(task_id)) {
                        task_status[task_id] = TaskStatus::FAILED;
                        scheduler_.mark_completed(task_id);
                        failed++;
                        completed++;
                    }
                }
            }

            // Poll for task completions and check timeouts
            std::vector<std::string> tasks_to_retry;
            for (auto& [task_id, status] : task_status) {
                if (status == TaskStatus::IN_PROGRESS && task_to_worker.count(task_id)) {
                    int worker_idx = task_to_worker[task_id];
                    
                    // Check timeout
                    if (fault_tolerance_.is_timed_out(task_id)) {
                        std::cerr << "Task " << task_id << " timed out (worker " 
                                  << worker_idx << ")" << std::endl;
                        fault_tolerance_.record_failure(task_id);
                        
                        if (fault_tolerance_.exceeded_max_retries(task_id)) {
                            status = TaskStatus::TIMEOUT;
                            scheduler_.mark_completed(task_id);
                            failed++;
                            completed++;
                            task_to_worker.erase(task_id);
                        } else {
                            tasks_to_retry.push_back(task_id);
                            task_to_worker.erase(task_id);
                            task_status[task_id] = TaskStatus::PENDING;
                        }
                        continue;
                    }
                    
                    // Check task status
                    StatusResponse status_resp;
                    bool status_ok = false;
                    try {
                        auto* worker = client_pool_.get_client(worker_idx);
                        if (worker) {
                            status_ok = worker->GetStatus(task_id, status_resp);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Exception getting status for task " << task_id 
                                  << " from worker " << worker_idx << ": " << e.what() << std::endl;
                        status_ok = false;
                    }
                    
                    if (!status_ok) {
                        // Worker may have failed, retry on different worker
                        std::cerr << "Worker " << worker_idx << " failed for task " 
                                  << task_id << ", scheduling retry" << std::endl;
                        fault_tolerance_.record_failure(task_id);
                        
                        if (fault_tolerance_.exceeded_max_retries(task_id)) {
                            status = TaskStatus::FAILED;
                            scheduler_.mark_completed(task_id);
                            failed++;
                            completed++;
                            task_to_worker.erase(task_id);
                        } else {
                            tasks_to_retry.push_back(task_id);
                            task_to_worker.erase(task_id);
                            task_status[task_id] = TaskStatus::PENDING;
                        }
                        continue;
                    }
                    
                    if (status_resp.status() == TaskStatus::COMPLETED ||
                        status_resp.status() == TaskStatus::FAILED) {
                        status = status_resp.status();
                        
                        // Get result and update cache
                        ResultResponse result;
                        bool result_ok = false;
                        try {
                            auto* worker = client_pool_.get_client(worker_idx);
                            if (worker) {
                                result_ok = worker->GetResult(task_id, result);
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "Exception getting result for task " << task_id 
                                      << ": " << e.what() << std::endl;
                        }
                        
                        if (result_ok && result.success() && redis_.is_available()) {
                            redis_.set(tasks[task_id].content_hash, result.artifact_path());
                        }
                        
                        if (status == TaskStatus::FAILED && 
                            !fault_tolerance_.exceeded_max_retries(task_id)) {
                            // Retry failed task
                            fault_tolerance_.record_failure(task_id);
                            tasks_to_retry.push_back(task_id);
                            task_to_worker.erase(task_id);
                            task_status[task_id] = TaskStatus::PENDING;
                            retried++;
                        } else {
                            // Task completed (success or final failure)
                            scheduler_.mark_completed(task_id);
                            completed++;
                            task_to_worker.erase(task_id);
                            if (status == TaskStatus::FAILED) {
                                failed++;
                            }
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Build completed: " << completed << " tasks, " 
                  << cached << " cached, " << retried << " retried, " 
                  << failed << " failed" << std::endl;
    }

private:
    coordinator::Scheduler scheduler_;
    coordinator::GrpcClientPool client_pool_;
    coordinator::RedisClient redis_;
    coordinator::FaultTolerance fault_tolerance_;
};

int main(int argc, char** argv) {
    std::string json_file = "build_graph.json";
    std::string redis_host = "redis";
    int redis_port = 6379;
    std::vector<std::string> worker_addresses;

    // Get worker addresses from environment (set by docker-compose)
    const char* env_workers = std::getenv("WORKER_ADDRESSES");
    if (env_workers) {
        std::stringstream ss(env_workers);
        std::string worker;
        while (std::getline(ss, worker, ',')) {
            worker_addresses.push_back(worker);
        }
    }

    // Default worker addresses if not set
    if (worker_addresses.empty()) {
        worker_addresses = {
            "worker-1:50052",
            "worker-2:50053",
            "worker-3:50054",
            "worker-4:50055",
            "worker-5:50056"
        };
    }

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc) {
            json_file = argv[++i];
        } else if (arg == "--redis-host" && i + 1 < argc) {
            redis_host = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            redis_port = std::stoi(argv[++i]);
        } else if (arg == "--workers" && i + 1 < argc) {
            worker_addresses.clear();
            std::string workers_str = argv[++i];
            std::stringstream ss(workers_str);
            std::string worker;
            while (std::getline(ss, worker, ',')) {
                worker_addresses.push_back(worker);
            }
        }
    }

    // Override Redis host/port from environment
    const char* env_redis_host = std::getenv("REDIS_HOST");
    if (env_redis_host) {
        redis_host = env_redis_host;
    }
    const char* env_redis_port = std::getenv("REDIS_PORT");
    if (env_redis_port) {
        redis_port = std::stoi(env_redis_port);
    }

    try {
        Coordinator coordinator(worker_addresses, redis_host, redis_port);
        coordinator.execute_build(json_file);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
