#include <iostream>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>

#include <grpcpp/grpcpp.h>
#include "build.grpc.pb.h"

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using build::BuildService;
using build::TaskRequest;
using build::TaskResponse;
using build::StatusRequest;
using build::StatusResponse;
using build::ResultRequest;
using build::ResultResponse;
using build::TaskStatus;

// Task execution state
struct TaskState {
    std::string task_id;
    TaskStatus status;
    std::string error_message;
    std::string artifact_path;
    std::string artifact_hash;
    bool success;
    int64_t build_time_ms;
    int64_t timestamp;
    std::mutex mutex;
    std::thread execution_thread;
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
        } else {
            std::cout << "Connected to Redis at " << host << ":" << port << std::endl;
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

    // Store artifact in cache
    void store_artifact(const std::string& content_hash, const std::string& artifact_hash, 
                       const std::string& artifact_path) {
#ifdef HAVE_REDIS
        if (!redis_) return;
        
        // Store mapping: content_hash -> artifact_path
        std::string key = "artifact:" + content_hash;
        redisReply* reply = (redisReply*)redisCommand(redis_, "SET %s %s", 
                                                      key.c_str(), artifact_path.c_str());
        if (reply) freeReplyObject(reply);
        
        // Store artifact metadata: artifact_hash -> metadata
        std::string meta_key = "meta:" + artifact_hash;
        std::string metadata = artifact_path + "|" + std::to_string(std::time(nullptr));
        reply = (redisReply*)redisCommand(redis_, "SET %s %s", meta_key.c_str(), metadata.c_str());
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

// Compute SHA256 hash of file or string
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

// Compute hash of file contents
std::string compute_file_hash(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return compute_hash(content);
}

// Mock compilation: simulate build process
void execute_mock_compile(TaskState* state, const TaskRequest& request, Cache& cache) {
    auto start_time = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->status = TaskStatus::IN_PROGRESS;
    }

    // Simulate compilation time (1-5 seconds based on number of source files)
    int sleep_ms = 1000 + (request.source_files_size() * 500);
    if (sleep_ms > 5000) sleep_ms = 5000;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    // Create output directory if needed
    std::filesystem::path output_path(request.output_path());
    std::filesystem::create_directories(output_path.parent_path());

    // Generate mock artifact (simulate compiled output)
    std::string artifact_content = "compiled:" + request.task_id();
    for (const auto& file : request.source_files()) {
        artifact_content += ":" + file;
    }
    
    // Write mock artifact
    std::ofstream outfile(request.output_path());
    if (outfile.is_open()) {
        outfile << artifact_content;
        outfile.close();
    }

    // Compute artifact hash
    std::string artifact_hash = compute_hash(artifact_content);
    
    auto end_time = std::chrono::steady_clock::now();
    auto build_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->status = TaskStatus::COMPLETED;
        state->success = true;
        state->artifact_path = request.output_path();
        state->artifact_hash = artifact_hash;
        state->build_time_ms = build_time;
        state->timestamp = std::time(nullptr);
    }

    // Store in Redis cache
    if (cache.is_available() && state->success) {
        cache.store_artifact(request.content_hash(), artifact_hash, request.output_path());
    }
}

// BuildService implementation
class BuildServiceImpl final : public BuildService::Service {
public:
    BuildServiceImpl(const std::string& redis_host, int redis_port)
        : cache_(redis_host, redis_port) {}

    Status SubmitTask(ServerContext* context, const TaskRequest* request,
                     TaskResponse* response) override {
        std::string task_id = request->task_id();
        
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            
            // Check if task already exists
            if (tasks_.count(task_id)) {
                response->set_accepted(false);
                response->set_message("Task already exists");
                response->set_status(TaskStatus::IN_PROGRESS);
                return Status::OK;
            }

            // Create new task state
            auto state = std::make_shared<TaskState>();
            state->task_id = task_id;
            state->status = TaskStatus::PENDING;
            state->success = false;
            
            tasks_[task_id] = state;

            // Start task execution in background thread
            state->execution_thread = std::thread(
                execute_mock_compile, state.get(), *request, std::ref(cache_));

            response->set_accepted(true);
            response->set_message("Task accepted");
            response->set_status(TaskStatus::PENDING);
        }

        std::cout << "Accepted task: " << task_id << std::endl;
        return Status::OK;
    }

    Status GetStatus(ServerContext* context, const StatusRequest* request,
                    StatusResponse* response) override {
        std::string task_id = request->task_id();
        
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        if (tasks_.count(task_id) == 0) {
            response->set_task_id(task_id);
            response->set_status(TaskStatus::FAILED);
            response->set_message("Task not found");
            return Status::OK;
        }

        auto state = tasks_[task_id];
        std::lock_guard<std::mutex> state_lock(state->mutex);
        
        response->set_task_id(task_id);
        response->set_status(state->status);
        
        if (state->status == TaskStatus::IN_PROGRESS) {
            response->set_progress_percent(50);  // Mock progress
            response->set_message("Building...");
        } else if (state->status == TaskStatus::COMPLETED) {
            response->set_progress_percent(100);
            response->set_message("Completed");
        } else if (state->status == TaskStatus::FAILED) {
            response->set_progress_percent(0);
            response->set_message(state->error_message);
        }

        return Status::OK;
    }

    Status GetResult(ServerContext* context, const ResultRequest* request,
                    ResultResponse* response) override {
        std::string task_id = request->task_id();
        
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        if (tasks_.count(task_id) == 0) {
            response->set_task_id(task_id);
            response->set_status(TaskStatus::FAILED);
            response->set_success(false);
            response->set_error_message("Task not found");
            return Status::OK;
        }

        auto state = tasks_[task_id];
        std::lock_guard<std::mutex> state_lock(state->mutex);
        
        response->set_task_id(task_id);
        response->set_status(state->status);
        response->set_success(state->success);
        
        if (state->success) {
            response->set_artifact_hash(state->artifact_hash);
            response->set_artifact_path(state->artifact_path);
            response->set_build_time_ms(state->build_time_ms);
            response->set_timestamp(state->timestamp);
        } else {
            response->set_error_message(state->error_message);
        }

        return Status::OK;
    }

    ~BuildServiceImpl() {
        // Wait for all tasks to complete
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        for (auto& [task_id, state] : tasks_) {
            if (state->execution_thread.joinable()) {
                state->execution_thread.join();
            }
        }
    }

private:
    std::unordered_map<std::string, std::shared_ptr<TaskState>> tasks_;
    std::mutex tasks_mutex_;
    Cache cache_;
};

void RunServer(const std::string& server_address, const std::string& redis_host, int redis_port) {
    BuildServiceImpl service(redis_host, redis_port);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Worker server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    std::string server_address = "0.0.0.0:50052";
    std::string redis_host = "redis";
    int redis_port = 6379;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            std::string port = argv[++i];
            server_address = "0.0.0.0:" + port;
        } else if (arg == "--host" && i + 1 < argc) {
            std::string host = argv[++i];
            size_t colon_pos = server_address.find(':');
            if (colon_pos != std::string::npos) {
                server_address = host + server_address.substr(colon_pos);
            } else {
                server_address = host + ":50052";
            }
        } else if (arg == "--redis-host" && i + 1 < argc) {
            redis_host = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            redis_port = std::stoi(argv[++i]);
        }
    }

    // Override from environment variables
    const char* env_host = std::getenv("WORKER_HOST");
    const char* env_port = std::getenv("WORKER_PORT");
    if (env_host && env_port) {
        server_address = std::string(env_host) + ":" + std::string(env_port);
    }

    const char* env_redis_host = std::getenv("REDIS_HOST");
    if (env_redis_host) {
        redis_host = env_redis_host;
    }
    const char* env_redis_port = std::getenv("REDIS_PORT");
    if (env_redis_port) {
        redis_port = std::stoi(env_redis_port);
    }

    RunServer(server_address, redis_host, redis_port);
    return 0;
}

