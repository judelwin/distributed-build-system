#include "redis_client.h"
#include <iostream>
#include <ctime>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

namespace worker {

RedisClient::RedisClient(const std::string& host, int port) {
#ifdef HAVE_REDIS
    redis_ = redisConnect(host.c_str(), port);
    if (redis_ == nullptr || static_cast<redisContext*>(redis_)->err) {
        std::cerr << "Failed to connect to Redis: " 
                  << (redis_ ? static_cast<redisContext*>(redis_)->errstr : "connection error") << std::endl;
        redis_ = nullptr;
    } else {
        std::cout << "Connected to Redis at " << host << ":" << port << std::endl;
    }
#else
    redis_ = nullptr;
#endif
}

RedisClient::~RedisClient() {
#ifdef HAVE_REDIS
    if (redis_) {
        redisFree(static_cast<redisContext*>(redis_));
    }
#endif
}

void RedisClient::store_artifact(const std::string& content_hash, const std::string& artifact_hash, 
                                 const std::string& artifact_path) {
#ifdef HAVE_REDIS
    if (!redis_) return;
    
    redisContext* ctx = static_cast<redisContext*>(redis_);
    
    // Store mapping: content_hash -> artifact_path
    std::string key = "artifact:" + content_hash;
    redisReply* reply = (redisReply*)redisCommand(ctx, "SET %s %s", 
                                                  key.c_str(), artifact_path.c_str());
    if (reply) freeReplyObject(reply);
    
    // Store artifact metadata: artifact_hash -> metadata
    std::string meta_key = "meta:" + artifact_hash;
    std::string metadata = artifact_path + "|" + std::to_string(std::time(nullptr));
    reply = (redisReply*)redisCommand(ctx, "SET %s %s", meta_key.c_str(), metadata.c_str());
    if (reply) freeReplyObject(reply);
#endif
}

bool RedisClient::is_available() const {
#ifdef HAVE_REDIS
    return redis_ != nullptr;
#else
    return false;
#endif
}

} // namespace worker

