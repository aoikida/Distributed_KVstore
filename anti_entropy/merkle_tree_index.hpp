#ifndef MERKLE_TREE_INDEX_HPP
#define MERKLE_TREE_INDEX_HPP

#include "../merklecpp/merklecpp.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include "../kv_store.hpp"

// Forward declaration
class KeyValueStore;

class MerkleTreeIndex {
public:
    MerkleTreeIndex();

    // Rebuild the Merkle tree from the current state of the key-value store
    void rebuild(const KeyValueStore& store);
    
    // Get the current Merkle root hash
    merkle::Hash get_root_hash() const;
    
    // Find differences between this tree and remote paths
    std::vector<std::string> find_differences(
        const std::vector<merkle::Path>& remote_paths,
        const std::vector<std::string>& keys);
    
    // Get paths for specific keys needed by remote nodes
    std::vector<merkle::Path> get_paths(
        const std::vector<std::string>& keys) const;
    
    // Check if the tree is empty
    bool empty() const;

    // Get the number of leaves in the tree
    size_t size() const;

private:
    // Generate a leaf hash from a key-value pair
    static merkle::Hash hash_key_value(
        const std::string& key, 
        const std::string& value,
        uint64_t timestamp);
    
    // The Merkle tree itself
    mutable std::mutex tree_mutex;
    merkle::Tree merkle_tree;
    
    // Maps keys to their positions in the tree
    std::unordered_map<std::string, size_t> key_to_index;
};

#endif // MERKLE_TREE_INDEX_HPP
