#ifndef KV_STORE_HPP
#define KV_STORE_HPP

#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>
#include <sstream>
#include <memory>

// Forward declaration
class MerkleTreeIndex;

class KeyValueStore {
    friend class MerkleTreeIndex;
public:
    struct ValueWithTimestamp {
        std::string value;
        uint64_t timestamp;
    };

    std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        return it != store_.end() ? it->second.value : "";
    }

    bool set(const std::string& key, const std::string& value, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end() || timestamp >= it->second.timestamp) {
            store_[key] = {value, timestamp};
            // Update Merkle tree if available
            if (merkle_index) {
                merkle_index->rebuild(*this);
            }
            return true;
        }
        return false;
    }

    bool del(const std::string& key, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it != store_.end() && timestamp >= it->second.timestamp) {
            store_.erase(it);
            // Update Merkle tree if available
            if (merkle_index) {
                merkle_index->rebuild(*this);
            }
            return true;
        }
        return false;
    }

    // Set the merkle tree index to use for this store
    void set_merkle_index(std::shared_ptr<MerkleTreeIndex> index) {
        std::lock_guard<std::mutex> lock(mutex_);
        merkle_index = index;
        if (merkle_index) {
            merkle_index->rebuild(*this);
        }
    }

    // For anti-entropy: get all keys with timestamps
    std::unordered_map<std::string, uint64_t> get_all_keys_with_timestamps() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        std::unordered_map<std::string, uint64_t> result;
        for (const auto& kv : store_) {
            result[kv.first] = kv.second.timestamp;
        }
        return result;
    }

    // For anti-entropy: get value with timestamp for a key
    ValueWithTimestamp get_value_with_timestamp(const std::string& key) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        auto it = store_.find(key);
        if (it != store_.end()) {
            return it->second;
        }
        return {"", 0};
    }
    
    // Process commands from client
    std::string process_command(const std::string& command) {
        std::istringstream iss(command);
        std::string action, key, value;
        iss >> action >> key >> value;
        
        if (action == "GET") {
            return get(key);
        } else if (action == "SET") {
            uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            bool success = set(key, value, timestamp);
            return success ? "OK" : "ERROR: Outdated timestamp";
        } else if (action == "DEL") {
            uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            bool success = del(key, timestamp);
            return success ? "OK" : "ERROR: Key not found or outdated timestamp";
        } else if (action == "GET_ALL") {
            auto keys_with_timestamps = get_all_keys_with_timestamps();
            std::string result;
            for (const auto& kv : keys_with_timestamps) {
                result += kv.first + ":" + std::to_string(kv.second) + ";";
            }
            return result;
        }
        return "ERROR: Invalid command";
    }

private:
    std::unordered_map<std::string, ValueWithTimestamp> store_;
    mutable std::mutex mutex_;
    std::shared_ptr<MerkleTreeIndex> merkle_index;
};

#endif // KV_STORE_HPP
