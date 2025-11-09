#pragma once

#include <string>

namespace coordinator {

// Redis cache interface for coordinator
class RedisClient {
public:
    RedisClient(const std::string& host, int port);
    ~RedisClient();

    // Check if artifact exists in cache by content hash
    bool exists(const std::string& hash);

    // Store artifact in cache
    void set(const std::string& hash, const std::string& artifact_path);

    bool is_available() const;

private:
#ifdef HAVE_REDIS
    void* redis_;  // redisContext* - using void* to avoid exposing hiredis in header
#endif
};

} // namespace coordinator

