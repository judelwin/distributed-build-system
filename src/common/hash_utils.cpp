#include "hash_utils.h"
#include "build_task.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace build {

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

std::string generate_content_hash(const Task& task) {
    std::stringstream ss;
    ss << task.id;
    for (const auto& file : task.source_files) ss << file;
    for (const auto& dep : task.dependencies) ss << dep;
    for (const auto& flag : task.compile_flags) ss << flag;
    ss << task.output_path;
    return compute_hash(ss.str());
}

} // namespace build

