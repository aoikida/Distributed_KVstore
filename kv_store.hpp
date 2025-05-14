#ifndef KV_STORE_HPP
#define KV_STORE_HPP

#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>
#include <sstream>
#include <memory>
#include "anti_entropy/index_interface.hpp"
#include <iostream>

class KeyValueStore {
public:
    struct ValueWithTimestamp {
        std::string value;
        uint64_t timestamp;
    };

    std::string get(const std::string& key) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = store_.find(key);
        return it != store_.end() ? it->second.value : "";
    }

    bool set(const std::string& key, const std::string& value, uint64_t timestamp) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end() || timestamp >= it->second.timestamp) {
            store_[key] = {value, timestamp};
            if (merkle_index) {
                merkle_index->rebuild(get_all_key_value_data());
            }
            return true;
        }
        return false;
    }

    bool del(const std::string& key, uint64_t timestamp) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it != store_.end() && timestamp >= it->second.timestamp) {
            store_.erase(it);
            if (merkle_index) {
                merkle_index->rebuild(get_all_key_value_data());
            }
            return true;
        }
        return false;
    }

    void set_merkle_index(std::shared_ptr<IndexInterface> index) {
        std::cout << "KeyValueStore::set_merkle_index: entered" << std::endl;
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        merkle_index = index;
        if (merkle_index) {
            std::cout << "KeyValueStore::set_merkle_index: calling rebuild" << std::endl;
            merkle_index->rebuild(get_all_key_value_data());
            std::cout << "KeyValueStore::set_merkle_index: rebuild done" << std::endl;
        }
    }

    IndexInterface::KeyValueData get_all_key_value_data() const {
        std::cout << "KeyValueStore::get_all_key_value_data: entered" << std::endl;
        std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(mutex_));
        IndexInterface::KeyValueData result;
        for (const auto& [key, value_ts] : store_) {
            result[key] = {value_ts.value, value_ts.timestamp};
        }
        std::cout << "KeyValueStore::get_all_key_value_data: returning" << std::endl;
        return result;
    }

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
            return set(key, value, timestamp) ? "OK" : "ERROR: Outdated timestamp";
        } else if (action == "DEL") {
            uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            return del(key, timestamp) ? "OK" : "ERROR: Key not found or outdated timestamp";
        } else if (action == "GET_ALL") {
            auto data = get_all_key_value_data();
            std::string result;
            for (const auto& [key, value_ts] : data) {
                result += key + ":" + std::to_string(value_ts.second) + ";";
            }
            return result;
        }
        return "ERROR: Invalid command";
    }

    std::vector<std::pair<std::string, uint64_t>> get_all_keys_with_timestamps() const {
        std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(mutex_));
        std::vector<std::pair<std::string, uint64_t>> result;
        for (const auto& [key, value_ts] : store_) {
            result.emplace_back(key, value_ts.timestamp);
        }
        return result;
    }

    ValueWithTimestamp get_value_with_timestamp(const std::string& key) const {
        std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(mutex_));
        auto it = store_.find(key);
        if (it != store_.end()) {
            return it->second;
        }
        return {"", 0};
    }

private:
    std::unordered_map<std::string, ValueWithTimestamp> store_;
    mutable std::recursive_mutex mutex_;
    std::shared_ptr<IndexInterface> merkle_index;
};

#endif // KV_STORE_HPP
