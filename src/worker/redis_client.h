#pragma once

#include <string>

namespace worker {

// Redis cache interface for worker
class RedisClient {
public:
    RedisClient(const std::string& host, int port);
    ~RedisClient();

    // Store artifact in cache
    void store_artifact(const std::string& content_hash, const std::string& artifact_hash, 
                       const std::string& artifact_path);

    bool is_available() const;

private:
#ifdef HAVE_REDIS
    void* redis_;  // redisContext* - using void* to avoid exposing hiredis in header
#endif
};

} // namespace worker

