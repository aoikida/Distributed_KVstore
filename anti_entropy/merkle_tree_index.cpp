#include "merkle_tree_index.hpp"
#include <sstream>
#include <iomanip>
#include <array>

MerkleTreeIndex::MerkleTreeIndex() {}

void MerkleTreeIndex::rebuild(const KeyValueStore& store) {
    std::lock_guard<std::mutex> guard(tree_mutex);
    
    // Clear existing tree
    merkle_tree = merkle::Tree();
    key_to_index.clear();
    
    // Get all key-value pairs with timestamps
    auto all_keys = store.get_all_keys_with_timestamps();
    size_t index = 0;
    
    // Insert each key-value pair into the tree
    for (const auto& [key, timestamp] : all_keys) {
        auto value_ts = store.get_value_with_timestamp(key);
        auto leaf_hash = hash_key_value(key, value_ts.value, timestamp);
        merkle_tree.insert(leaf_hash);
        key_to_index[key] = index++;
    }
    
    std::cout << "Rebuilt Merkle tree with " << index << " key-value pairs" << std::endl;
}

merkle::Hash MerkleTreeIndex::get_root_hash() const {
    std::lock_guard<std::mutex> guard(tree_mutex);
    
    if (merkle_tree.empty()) {
        // Return an empty hash if the tree is empty
        return merkle::Hash();
    }
    
    return merkle_tree.root();
}

std::vector<std::string> MerkleTreeIndex::find_differences(
    const std::vector<merkle::Path>& remote_paths,
    const std::vector<std::string>& keys) {
    
    std::lock_guard<std::mutex> guard(tree_mutex);
    std::vector<std::string> differing_keys;
    
    if (merkle_tree.empty()) {
        return keys; // If our tree is empty, all keys differ
    }
    
    // Get our root
    auto local_root = merkle_tree.root();
    
    // Compare paths
    for (size_t i = 0; i < remote_paths.size(); i++) {
        const auto& path = remote_paths[i];
        
        // If the path doesn't verify against our root, the key differs
        if (!path.verify(local_root)) {
            if (i < keys.size()) {
                differing_keys.push_back(keys[i]);
            }
        }
    }
    
    return differing_keys;
}

std::vector<merkle::Path> MerkleTreeIndex::get_paths(
    const std::vector<std::string>& keys) const {
    
    std::lock_guard<std::mutex> guard(tree_mutex);
    std::vector<merkle::Path> paths;
    
    if (merkle_tree.empty()) {
        return paths;
    }
    
    for (const auto& key : keys) {
        auto it = key_to_index.find(key);
        if (it != key_to_index.end()) {
            // Get the Merkle path for this key's position
            paths.push_back(*merkle_tree.path(it->second));
        }
    }
    
    return paths;
}

bool MerkleTreeIndex::empty() const {
    std::lock_guard<std::mutex> guard(tree_mutex);
    return merkle_tree.empty();
}

size_t MerkleTreeIndex::size() const {
    std::lock_guard<std::mutex> guard(tree_mutex);
    return merkle_tree.num_leaves();
}

merkle::Hash MerkleTreeIndex::hash_key_value(
    const std::string& key, 
    const std::string& value,
    uint64_t timestamp) {
    
    // Concatenate key, value, and timestamp
    std::ostringstream oss;
    oss << key << ":" << value << ":" << timestamp;
    std::string combined = oss.str();
    
    // Create a new empty hash
    merkle::Hash left;
    
    // Fill the left hash with bytes from our combined string
    // (This is simplified, but works for demonstration)
    size_t copy_size = std::min(combined.size(), (size_t)32);
    std::memcpy(left.bytes, combined.c_str(), copy_size);
    
    // Use merkle library's hash function to derive a proper hash
    merkle::Hash right; // Empty hash
    merkle::Hash result;
    merkle::sha256_compress(left, right, result);
    
    return result;
}
