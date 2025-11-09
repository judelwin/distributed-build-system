#include "redis_client.h"
#include <iostream>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

namespace coordinator {

RedisClient::RedisClient(const std::string& host, int port) {
#ifdef HAVE_REDIS
    redis_ = redisConnect(host.c_str(), port);
    if (redis_ == nullptr || static_cast<redisContext*>(redis_)->err) {
        std::cerr << "Failed to connect to Redis: " 
                  << (redis_ ? static_cast<redisContext*>(redis_)->errstr : "connection error") << std::endl;
        redis_ = nullptr;
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

bool RedisClient::exists(const std::string& hash) {
#ifdef HAVE_REDIS
    if (!redis_) return false;
    redisContext* ctx = static_cast<redisContext*>(redis_);
    redisReply* reply = (redisReply*)redisCommand(ctx, "EXISTS %s", hash.c_str());
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        bool result = reply->integer == 1;
        freeReplyObject(reply);
        return result;
    }
    if (reply) freeReplyObject(reply);
#endif
    return false;
}

void RedisClient::set(const std::string& hash, const std::string& artifact_path) {
#ifdef HAVE_REDIS
    if (!redis_) return;
    redisContext* ctx = static_cast<redisContext*>(redis_);
    redisReply* reply = (redisReply*)redisCommand(ctx, "SET %s %s", 
                                                  hash.c_str(), artifact_path.c_str());
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

} // namespace coordinator

