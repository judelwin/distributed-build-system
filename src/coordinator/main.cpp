#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <cstring>
#include <cstdlib>

#include <grpcpp/grpcpp.h>
#include "build.grpc.pb.h"

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using build::BuildService;
using build::TaskRequest;
using build::TaskResponse;
using build::StatusRequest;
using build::StatusResponse;
using build::ResultRequest;
using build::ResultResponse;
using build::TaskStatus;

// Task structure for internal scheduling
struct Task {
    std::string id;
    std::string content_hash;
    std::vector<std::string> source_files;
    std::vector<std::string> dependencies;
    std::vector<std::string> compile_flags;
    std::string output_path;
    int timeout_seconds;
    std::unordered_set<std::string> depends_on;  // Task IDs this depends on
    int in_degree;  // Number of unresolved dependencies
    TaskStatus status;
};

// gRPC client wrapper for worker communication
class WorkerClient {
public:
    WorkerClient(std::shared_ptr<Channel> channel)
        : stub_(BuildService::NewStub(channel)) {}

    bool SubmitTask(const TaskRequest& request, TaskResponse& response) {
        ClientContext context;
        Status status = stub_->SubmitTask(&context, request, &response);
        return status.ok() && response.accepted();
    }

    bool GetStatus(const std::string& task_id, StatusResponse& response) {
        ClientContext context;
        StatusRequest request;
        request.set_task_id(task_id);
        Status status = stub_->GetStatus(&context, request, &response);
        return status.ok();
    }

    bool GetResult(const std::string& task_id, ResultResponse& response) {
        ClientContext context;
        ResultRequest request;
        request.set_task_id(task_id);
        Status status = stub_->GetResult(&context, request, &response);
        return status.ok();
    }

private:
    std::unique_ptr<BuildService::Stub> stub_;
};

// Redis cache interface
class Cache {
public:
    Cache(const std::string& host, int port) {
#ifdef HAVE_REDIS
        redis_ = redisConnect(host.c_str(), port);
        if (redis_ == nullptr || redis_->err) {
            std::cerr << "Failed to connect to Redis: " 
                      << (redis_ ? redis_->errstr : "connection error") << std::endl;
            redis_ = nullptr;
        }
#endif
    }

    ~Cache() {
#ifdef HAVE_REDIS
        if (redis_) {
            redisFree(redis_);
        }
#endif
    }

    // Check if artifact exists in cache by content hash
    bool exists(const std::string& hash) {
#ifdef HAVE_REDIS
        if (!redis_) return false;
        redisReply* reply = (redisReply*)redisCommand(redis_, "EXISTS %s", hash.c_str());
        if (reply && reply->type == REDIS_REPLY_INTEGER) {
            bool result = reply->integer == 1;
            freeReplyObject(reply);
            return result;
        }
        if (reply) freeReplyObject(reply);
#endif
        return false;
    }

    // Store artifact in cache
    void set(const std::string& hash, const std::string& artifact_path) {
#ifdef HAVE_REDIS
        if (!redis_) return;
        redisReply* reply = (redisReply*)redisCommand(redis_, "SET %s %s", 
                                                      hash.c_str(), artifact_path.c_str());
        if (reply) freeReplyObject(reply);
#endif
    }

    bool is_available() const {
#ifdef HAVE_REDIS
        return redis_ != nullptr;
#else
        return false;
#endif
    }

private:
#ifdef HAVE_REDIS
    redisContext* redis_;
#endif
};

// Compute SHA256 hash of string
std::string compute_hash(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// Generate content hash from task properties
std::string generate_content_hash(const Task& task) {
    std::stringstream ss;
    ss << task.id;
    for (const auto& file : task.source_files) ss << file;
    for (const auto& dep : task.dependencies) ss << dep;
    for (const auto& flag : task.compile_flags) ss << flag;
    ss << task.output_path;
    return compute_hash(ss.str());
}

// Parse JSON dependency graph
std::unordered_map<std::string, Task> parse_dependency_graph(const std::string& json_file) {
    std::ifstream file(json_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + json_file);
    }

    json j;
    file >> j;

    std::unordered_map<std::string, Task> tasks;

    // Parse tasks from JSON
    if (j.contains("tasks") && j["tasks"].is_array()) {
        for (const auto& task_json : j["tasks"]) {
            Task task;
            task.id = task_json["id"];
            task.output_path = task_json.value("output_path", "");
            
            if (task_json.contains("source_files")) {
                for (const auto& file : task_json["source_files"]) {
                    task.source_files.push_back(file);
                }
            }
            
            if (task_json.contains("dependencies")) {
                for (const auto& dep : task_json["dependencies"]) {
                    task.dependencies.push_back(dep);
                }
            }
            
            if (task_json.contains("compile_flags")) {
                for (const auto& flag : task_json["compile_flags"]) {
                    task.compile_flags.push_back(flag);
                }
            }
            
            task.timeout_seconds = task_json.value("timeout_seconds", 60);
            task.status = TaskStatus::PENDING;
            
            // Parse task dependencies (other task IDs)
            if (task_json.contains("depends_on")) {
                for (const auto& dep_id : task_json["depends_on"]) {
                    task.depends_on.insert(dep_id);
                }
            }
            
            task.in_degree = task.depends_on.size();
            task.content_hash = generate_content_hash(task);
            tasks[task.id] = task;
        }
    }

    return tasks;
}

// Topological sort to determine build order
std::vector<std::string> topological_sort(std::unordered_map<std::string, Task>& tasks) {
    // Build reverse dependency graph (which tasks depend on this task)
    std::unordered_map<std::string, std::vector<std::string>> reverse_deps;
    for (const auto& [id, task] : tasks) {
        for (const auto& dep_id : task.depends_on) {
            reverse_deps[dep_id].push_back(id);
        }
    }

    std::queue<std::string> ready_queue;
    std::unordered_map<std::string, int> in_degree;
    std::vector<std::string> result;

    // Initialize in-degree counts
    for (const auto& [id, task] : tasks) {
        in_degree[id] = task.in_degree;
        if (task.in_degree == 0) {
            ready_queue.push(id);
        }
    }

    // Process ready tasks
    while (!ready_queue.empty()) {
        std::string current = ready_queue.front();
        ready_queue.pop();
        result.push_back(current);

        // Reduce in-degree for tasks that depend on current
        if (reverse_deps.count(current)) {
            for (const auto& dependent_id : reverse_deps[current]) {
                in_degree[dependent_id]--;
                if (in_degree[dependent_id] == 0) {
                    ready_queue.push(dependent_id);
                }
            }
        }
    }

    // Check for cycles
    if (result.size() != tasks.size()) {
        throw std::runtime_error("Dependency graph contains cycles");
    }

    return result;
}

// Coordinator class
class Coordinator {
public:
    Coordinator(const std::vector<std::string>& worker_addresses,
                const std::string& redis_host, int redis_port)
        : cache_(redis_host, redis_port), current_worker_(0) {
        
        // Create gRPC clients for each worker
        for (const auto& addr : worker_addresses) {
            auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
            workers_.push_back(std::make_unique<WorkerClient>(channel));
            worker_loads_.push_back(0);
        }
    }

    // Schedule and execute build tasks
    void execute_build(const std::string& json_file) {
        auto tasks = parse_dependency_graph(json_file);
        
        std::cout << "Parsed " << tasks.size() << " tasks" << std::endl;

        std::unordered_map<std::string, TaskStatus> task_status;
        std::unordered_map<std::string, int> remaining_deps;
        std::unordered_map<std::string, int> task_to_worker;  // Track which worker handles which task
        std::mutex status_mutex;
        std::queue<std::string> ready_queue;
        int completed = 0;
        int cached = 0;

        // Build reverse dependency graph and initialize counts
        std::unordered_map<std::string, std::vector<std::string>> reverse_deps;
        for (const auto& [id, task] : tasks) {
            remaining_deps[id] = task.in_degree;
            if (task.in_degree == 0) {
                ready_queue.push(id);
            }
            for (const auto& dep_id : task.depends_on) {
                reverse_deps[dep_id].push_back(id);
            }
        }

        // Process ready tasks (can execute in parallel)
        while (completed < tasks.size()) {
            std::vector<std::string> tasks_to_process;
            
            // Collect all ready tasks
            {
                std::lock_guard<std::mutex> lock(status_mutex);
                while (!ready_queue.empty() && tasks_to_process.size() < workers_.size()) {
                    tasks_to_process.push_back(ready_queue.front());
                    ready_queue.pop();
                }
            }

            // Process each ready task
            for (const auto& task_id : tasks_to_process) {
                auto& task = tasks[task_id];
                
                // Check cache before scheduling
                if (cache_.is_available() && cache_.exists(task.content_hash)) {
                    std::cout << "Task " << task_id << " found in cache (hash: " 
                              << task.content_hash.substr(0, 8) << ")" << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(status_mutex);
                        task_status[task_id] = TaskStatus::COMPLETED;
                        cached++;
                        completed++;
                        
                        // Mark dependent tasks as ready if all dependencies completed
                        if (reverse_deps.count(task_id)) {
                            for (const auto& dependent_id : reverse_deps[task_id]) {
                                remaining_deps[dependent_id]--;
                                if (remaining_deps[dependent_id] == 0 && 
                                    task_status.count(dependent_id) == 0) {
                                    ready_queue.push(dependent_id);
                                }
                            }
                        }
                    }
                    continue;
                }

                // Select worker (round-robin)
                std::lock_guard<std::mutex> lock(worker_mutex_);
                int worker_idx = select_worker();
                auto& worker = workers_[worker_idx];
                worker_loads_[worker_idx]++;

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

                // Submit task to worker (async execution)
                TaskResponse response;
                if (worker->SubmitTask(request, response)) {
                    std::cout << "Submitted task " << task_id << " to worker " 
                              << worker_idx << std::endl;
                    task_status[task_id] = TaskStatus::IN_PROGRESS;
                    task_to_worker[task_id] = worker_idx;
                } else {
                    std::cerr << "Failed to submit task " << task_id << std::endl;
                    task_status[task_id] = TaskStatus::FAILED;
                    worker_loads_[worker_idx]--;
                    {
                        std::lock_guard<std::mutex> lock(status_mutex);
                        completed++;
                    }
                    continue;
                }
            }

            // Poll for task completions
            for (auto& [task_id, status] : task_status) {
                if (status == TaskStatus::IN_PROGRESS && task_to_worker.count(task_id)) {
                    int worker_idx = task_to_worker[task_id];
                    StatusResponse status_resp;
                    if (workers_[worker_idx]->GetStatus(task_id, status_resp)) {
                        if (status_resp.status() == TaskStatus::COMPLETED ||
                            status_resp.status() == TaskStatus::FAILED) {
                            status = status_resp.status();
                            
                            // Get result and update cache
                            ResultResponse result;
                            if (workers_[worker_idx]->GetResult(task_id, result)) {
                                if (result.success() && cache_.is_available()) {
                                    cache_.set(tasks[task_id].content_hash, result.artifact_path());
                                }
                            }
                            
                            {
                                std::lock_guard<std::mutex> lock(status_mutex);
                                worker_loads_[worker_idx]--;
                                completed++;
                                
                                // Mark dependent tasks as ready
                                if (reverse_deps.count(task_id)) {
                                    for (const auto& dependent_id : reverse_deps[task_id]) {
                                        remaining_deps[dependent_id]--;
                                        if (remaining_deps[dependent_id] == 0 && 
                                            task_status.count(dependent_id) == 0) {
                                            ready_queue.push(dependent_id);
                                        }
                                    }
                                }
                            }
                            task_to_worker.erase(task_id);
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Build completed: " << completed << " tasks, " 
                  << cached << " cached" << std::endl;
    }

private:
    int select_worker() {
        // Round-robin selection (mutex already held by caller)
        int selected = current_worker_;
        current_worker_ = (current_worker_ + 1) % workers_.size();
        return selected;
    }

    Cache cache_;
    std::vector<std::unique_ptr<WorkerClient>> workers_;
    std::vector<int> worker_loads_;
    int current_worker_;
    std::mutex worker_mutex_;
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

