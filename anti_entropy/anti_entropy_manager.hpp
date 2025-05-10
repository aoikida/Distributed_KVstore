#ifndef ANTI_ENTROPY_MANAGER_HPP
#define ANTI_ENTROPY_MANAGER_HPP

#include <boost/asio.hpp>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include <chrono>
#include <memory>
#include "kv_store.hpp"
#include "merkle_tree_index.hpp"

using boost::asio::ip::tcp;

class Node;  // Forward declaration

class AntiEntropyManager {
    friend class Node;  // Grant Node access to merkle_index_
    
public:
    enum SyncMode {
        FULL_STATE_EXCHANGE, // Original method with direct key-value exchange
        MERKLE_TREE          // New method using Merkle trees for efficient sync
    };
public:
    AntiEntropyManager(boost::asio::io_context& io_context, KeyValueStore& kv_store,
                       const std::string& peer_host, short peer_port,
                       SyncMode mode = MERKLE_TREE)
        : io_context_(io_context), kv_store_(kv_store),
          peer_host_(peer_host), peer_port_(peer_port), sync_mode_(mode) 
    {
        // Initialize Merkle tree index if using Merkle tree sync mode
        if (sync_mode_ == MERKLE_TREE) {
            merkle_index_ = std::make_shared<MerkleTreeIndex>();
            kv_store_.set_merkle_index(merkle_index_);
        }
    }

    void start() {
        anti_entropy_thread_ = std::thread([this]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                run_anti_entropy();
            }
        });
        anti_entropy_thread_.detach();
    }

    // Main anti-entropy method - delegates to appropriate implementation
    void run_anti_entropy() {
        if (sync_mode_ == MERKLE_TREE) {
            run_merkle_anti_entropy();
        } else {
            run_full_state_anti_entropy();
        }
    }

    // Merkle tree-based anti-entropy method
    void run_merkle_anti_entropy() {
        try {
            std::cout << "Starting Merkle tree anti-entropy cycle..." << std::endl;
            
            if (!merkle_index_ || merkle_index_->empty()) {
                std::cout << "Local Merkle tree is empty, falling back to full state exchange..." << std::endl;
                run_full_state_anti_entropy();
                return;
            }
            
            // Step 1: Get local Merkle root
            auto local_root = merkle_index_->get_root_hash();
            std::cout << "Local Merkle root: " << local_root.to_string().substr(0, 8) << "..." << std::endl;
            
            // Step 2: Get peer's Merkle root
            auto peer_root = request_peer_merkle_root();
            if (peer_root.to_string() == "0000000000000000000000000000000000000000000000000000000000000000") {
                std::cout << "Peer has no Merkle tree, falling back to full state exchange..." << std::endl;
                run_full_state_anti_entropy();
                return;
            }
            
            std::cout << "Peer Merkle root: " << peer_root.to_string().substr(0, 8) << "..." << std::endl;
            
            // Step 3: If roots are identical, stores are in sync
            if (local_root.to_string() == peer_root.to_string()) {
                std::cout << "Merkle roots match, stores are in sync" << std::endl;
                return;
            }
            
            // Step 4: Roots differ, identify the differences
            std::cout << "Merkle roots differ, finding differences..." << std::endl;
            
            // Get all local keys
            auto local_keys = kv_store_.get_all_keys_with_timestamps();
            std::vector<std::string> key_list;
            for (const auto& [key, _] : local_keys) {
                key_list.push_back(key);
            }
            
            // Get Merkle paths from peer for these keys
            auto peer_paths = request_peer_paths(key_list);
            
            // Find exactly which keys differ
            auto differing_keys = merkle_index_->find_differences(peer_paths, key_list);
            
            // Step 5: Resolve differences for the identified keys
            if (!differing_keys.empty()) {
                std::cout << "Found " << differing_keys.size() << " different keys" << std::endl;
                
                // Fetch values from peer for these keys
                for (const auto& key : differing_keys) {
                    fetch_and_update_key(key);
                }
                
                std::cout << "Successfully synchronized differing keys" << std::endl;
            }
            
        } catch (std::exception& e) {
            std::cerr << "Merkle anti-entropy error: " << e.what() << std::endl;
            
            // Fall back to full state exchange on error
            std::cout << "Falling back to full state exchange..." << std::endl;
            run_full_state_anti_entropy();
        }
    }

    // Original anti-entropy method
    void run_full_state_anti_entropy() {
        try {
            std::cout << "Starting anti-entropy cycle..." << std::endl;
            
            // Step 1: Get local keys and timestamps
            auto local_keys = kv_store_.get_all_keys_with_timestamps();
            
            if (local_keys.empty()) {
                std::cout << "Local store is empty, checking if peer has any data..." << std::endl;
                auto peer_keys = get_peer_keys_with_timestamps();
                
                if (!peer_keys.empty()) {
                    std::cout << "Peer has data, fetching..." << std::endl;
                    for (const auto& [key, ts] : peer_keys) {
                        fetch_and_update_key(key);
                    }
                }
                return;
            }

            // Step 2: Get peer's keys and timestamps
            auto peer_keys = get_peer_keys_with_timestamps();
            
            if (peer_keys.empty()) {
                std::cout << "Peer has no data, sending our data..." << std::endl;
                for (const auto& [key, ts] : local_keys) {
                    send_update_to_peer(key);
                }
                return;
            }
            
            // Step 3: Compare keys to find differences
            bool differences_found = false;
            
            // Sync local keys to peer
            for (const auto& [key, local_ts] : local_keys) {
                auto it = peer_keys.find(key);
                
                if (it == peer_keys.end()) {
                    // Key exists locally but not on peer, send to peer
                    differences_found = true;
                    std::cout << "Key '" << key << "' exists locally but not on peer, sending..." << std::endl;
                    send_update_to_peer(key);
                } else if (local_ts > it->second) {
                    // Local version is newer, send to peer
                    differences_found = true;
                    std::cout << "Local version of key '" << key << "' is newer, sending to peer..." << std::endl;
                    send_update_to_peer(key);
                }
            }
            
            // Sync peer keys to local
            for (const auto& [key, peer_ts] : peer_keys) {
                auto it = local_keys.find(key);
                
                if (it == local_keys.end()) {
                    // Key exists on peer but not locally, fetch it
                    differences_found = true;
                    std::cout << "Key '" << key << "' exists on peer but not locally, fetching..." << std::endl;
                    fetch_and_update_key(key);
                } else if (peer_ts > it->second) {
                    // Peer version is newer, fetch it
                    differences_found = true;
                    std::cout << "Peer version of key '" << key << "' is newer, fetching..." << std::endl;
                    fetch_and_update_key(key);
                }
            }
            
            if (!differences_found) {
                std::cout << "No differences found, stores are in sync" << std::endl;
            } else {
                std::cout << "Anti-entropy synchronization completed successfully" << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << "Anti-entropy error: " << e.what() << std::endl;
        }
    }

private:
    std::unordered_map<std::string, uint64_t> get_peer_keys_with_timestamps() {
        std::unordered_map<std::string, uint64_t> result;
        
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
            
            // Request all keys with timestamps
            std::string request = "GET_ALL";
            boost::asio::write(socket, boost::asio::buffer(request));
            
            // Read response
            std::array<char, 8192> buffer;
            size_t length = socket.read_some(boost::asio::buffer(buffer));
            std::string response(buffer.data(), length);
            
            // Parse response into key-timestamp pairs
            parse_keys_with_timestamps(response, result);
        } catch (std::exception& e) {
            std::cerr << "Error getting peer's keys: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    void fetch_and_update_key(const std::string& key) {
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
            
            // Request the value for a specific key
            std::string request = "GET " + key;
            boost::asio::write(socket, boost::asio::buffer(request));
            
            // Read response
            std::array<char, 4096> buffer;
            size_t length = socket.read_some(boost::asio::buffer(buffer));
            std::string value(buffer.data(), length);
            
            if (!value.empty()) {
                // For better timestamp handling, we should ideally include the timestamp
                // in the response, but for simplicity we'll use current time + small offset
                uint64_t timestamp = current_timestamp() + 1;
                kv_store_.set(key, value, timestamp);
                std::cout << "Updated key from peer: " << key << " = " << value << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << "Failed to fetch and update key " << key << ": " << e.what() << std::endl;
        }
    }
    
    void parse_keys_with_timestamps(const std::string& data, std::unordered_map<std::string, uint64_t>& out_map) {
        if (data.empty()) return;
        
        // Format: key1:timestamp1;key2:timestamp2;...
        size_t start = 0;
        while (start < data.size()) {
            size_t sep = data.find(':', start);
            if (sep == std::string::npos) break;
            
            std::string key = data.substr(start, sep - start);
            size_t end = data.find(';', sep);
            
            std::string ts_str;
            if (end == std::string::npos) {
                ts_str = data.substr(sep + 1);
            } else {
                ts_str = data.substr(sep + 1, end - sep - 1);
            }
            
            try {
                uint64_t ts = std::stoull(ts_str);
                out_map[key] = ts;
            } catch (std::exception& e) {
                std::cerr << "Error parsing timestamp for key " << key << ": " << e.what() << std::endl;
            }
            
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    
    void send_update_to_peer(const std::string& key) {
        try {
            auto val_ts = kv_store_.get_value_with_timestamp(key);
            std::string command = "PROPAGATE SET " + key + " " + val_ts.value + " " + std::to_string(val_ts.timestamp);
            propagate_update(command);
        } catch (std::exception& e) {
            std::cerr << "Failed to send update to peer for key " << key << ": " << e.what() << std::endl;
        }
    }
    
    void propagate_update(const std::string& command) {
        if (!peer_host_.empty() && peer_port_ > 0) {
            try {
                tcp::socket socket(io_context_);
                tcp::resolver resolver(io_context_);
                boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
                boost::asio::write(socket, boost::asio::buffer(command));
                std::cout << "Propagated update to peer: " << command << std::endl;
            } catch (std::exception& e) {
                std::cerr << "Failed to propagate update: " << e.what() << std::endl;
            }
        }
    }
    
    // Request the Merkle root hash from peer
    merkle::Hash request_peer_merkle_root() {
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
            
            // Request the Merkle root
            std::string request = "GET_MERKLE_ROOT";
            boost::asio::write(socket, boost::asio::buffer(request));
            
            // Read response
            std::array<char, 1024> buffer;
            size_t length = socket.read_some(boost::asio::buffer(buffer));
            std::string response(buffer.data(), length);
            
            // If empty, peer doesn't have a Merkle tree yet
            if (response.empty() || response == "EMPTY") {
                return merkle::Hash(); // Empty hash
            }
            
            return merkle::Hash(response);
        } catch (std::exception& e) {
            std::cerr << "Error getting peer's Merkle root: " << e.what() << std::endl;
            return merkle::Hash(); // Empty hash on error
        }
    }
    
    // Request Merkle paths from peer for the given keys
    std::vector<merkle::Path> request_peer_paths(const std::vector<std::string>& keys) {
        std::vector<merkle::Path> paths;
        
        if (keys.empty()) {
            return paths;
        }
        
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
            
            // Build the request
            std::string request = "GET_PATHS ";
            for (const auto& key : keys) {
                request += key + ";";
            }
            
            // Send request
            boost::asio::write(socket, boost::asio::buffer(request));
            
            // Read response
            std::array<char, 16384> buffer; // Larger buffer for paths
            size_t length = socket.read_some(boost::asio::buffer(buffer));
            std::string response(buffer.data(), length);
            
            // Parse paths from response
            // Format: key1,serialized_path1;key2,serialized_path2;...
            size_t start = 0;
            while (start < response.size()) {
                size_t sep = response.find(',', start);
                if (sep == std::string::npos) break;
                
                std::string key = response.substr(start, sep - start);
                size_t end = response.find(';', sep);
                
                std::string path_str;
                if (end == std::string::npos) {
                    path_str = response.substr(sep + 1);
                } else {
                    path_str = response.substr(sep + 1, end - sep - 1);
                }
                
                // Convert path string to binary
                std::vector<uint8_t> path_bytes;
                for (size_t i = 0; i < path_str.size(); i += 2) {
                    path_bytes.push_back(std::stoi(path_str.substr(i, 2), nullptr, 16));
                }
                
                // Deserialize path
                paths.push_back(merkle::Path(path_bytes));
                
                if (end == std::string::npos) break;
                start = end + 1;
            }
        } catch (std::exception& e) {
            std::cerr << "Error getting peer's Merkle paths: " << e.what() << std::endl;
        }
        
        return paths;
    }

    uint64_t current_timestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    boost::asio::io_context& io_context_;
    KeyValueStore& kv_store_;
    std::string peer_host_;
    short peer_port_;
    SyncMode sync_mode_;
    std::shared_ptr<MerkleTreeIndex> merkle_index_;
    std::thread anti_entropy_thread_;
};

#endif // ANTI_ENTROPY_MANAGER_HPP
