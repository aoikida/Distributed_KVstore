#ifndef MERKLE_TREE_INDEX_HPP
#define MERKLE_TREE_INDEX_HPP

#include "../merklecpp/merklecpp.h"
#include "../anti_entropy/index_interface.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <iostream>

class MerkleTreeIndex : public IndexInterface {
public:
    MerkleTreeIndex() = default;

    void rebuild(const KeyValueData& kv_data) override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        
        // Clear existing tree
        merkle_tree = merkle::Tree();
        key_to_index.clear();
        
        // Insert each key-value pair from the interface data
        size_t index = 0;
        for (const auto& [key, value_ts] : kv_data) {
            auto leaf_hash = hash_key_value(key, value_ts.first, value_ts.second);
            merkle_tree.insert(leaf_hash);
            key_to_index[key] = index++;
        }
        
        std::cout << "Rebuilt Merkle tree with " << index << " key-value pairs" << std::endl;
    }
    
    merkle::Hash get_root_hash() const override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        return merkle_tree.empty() ? merkle::Hash() : const_cast<merkle::Tree&>(merkle_tree).root();
    }
    
    std::vector<std::string> find_differences(
        const std::vector<merkle::Path>& remote_paths,
        const std::vector<std::string>& keys) {
        std::lock_guard<std::mutex> guard(tree_mutex);
        std::vector<std::string> differing_keys;
        
        if (!merkle_tree.empty()) {
            auto local_root = merkle_tree.root();
            for (size_t i = 0; i < remote_paths.size() && i < keys.size(); i++) {
                if (!remote_paths[i].verify(local_root)) {
                    differing_keys.push_back(keys[i]);
                }
            }
        }
        return differing_keys;
    }
    
    std::vector<merkle::Path> get_paths(const std::vector<std::string>& keys) const override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        std::vector<merkle::Path> paths;
        
        if (!merkle_tree.empty()) {
            for (const auto& key : keys) {
                auto it = key_to_index.find(key);
                if (it != key_to_index.end()) {
                    paths.push_back(*const_cast<merkle::Tree&>(merkle_tree).path(it->second));
                }
            }
        }
        return paths;
    }
    
    size_t size() const override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        return merkle_tree.num_leaves();
    }
    
    bool empty() const override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        return merkle_tree.empty();
    }

    std::unordered_map<std::string, uint64_t> get_key_timestamps() const override {
        std::lock_guard<std::mutex> guard(tree_mutex);
        std::unordered_map<std::string, uint64_t> result;
        for (const auto& [key, idx] : key_to_index) {
            result[key] = 0; // Placeholder, update if you have actual timestamps
        }
        return result;
    }

private:
    static merkle::Hash hash_key_value(const std::string& key, 
                                      const std::string& value,
                                      uint64_t timestamp) {
        std::ostringstream oss;
        oss << key << ":" << value << ":" << timestamp;
        std::string combined = oss.str();
        
        merkle::Hash left;
        size_t copy_size = std::min(combined.size(), (size_t)32);
        std::memcpy(left.bytes, combined.c_str(), copy_size);
        
        merkle::Hash right; // Empty hash
        merkle::Hash result;
        merkle::sha256_compress(left, right, result);
        return result;
    }

    mutable std::mutex tree_mutex;
    merkle::Tree merkle_tree;
    std::unordered_map<std::string, size_t> key_to_index;
};

#endif // MERKLE_TREE_INDEX_HPP
