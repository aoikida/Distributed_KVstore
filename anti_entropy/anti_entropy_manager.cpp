#include "anti_entropy_manager.hpp"
#include "merkle_tree_index.hpp"
#include "kv_store.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>

// Implementation file for AntiEntropyManager
// All methods are implemented inline in the header for simplicity.
// If needed, move method implementations here from the header.

AntiEntropyManager::AntiEntropyManager(boost::asio::io_context& io_context, KeyValueStore& kv_store,
                                       const std::string& peer_host, short peer_port,
                                       std::shared_ptr<IndexInterface> merkle_index,
                                       SyncMode mode)
    : io_context_(io_context), kv_store_(kv_store), peer_host_(peer_host), peer_port_(peer_port), sync_mode_(mode), merkle_index_(merkle_index) {}

void AntiEntropyManager::start() {
    anti_entropy_thread_ = std::thread([this]() {
        while (true) {
            try {
                run_anti_entropy();
            } catch (const std::exception& e) {
                std::cerr << "Anti-entropy error: " << e.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
}

void AntiEntropyManager::run_anti_entropy() {
    std::cout << "[AntiEntropy] Running anti-entropy sync..." << std::endl;
    // 1. Get local Merkle root
    auto local_index = std::dynamic_pointer_cast<MerkleTreeIndex>(merkle_index_);
    if (!local_index) {
        std::cout << "[AntiEntropy] Local Merkle index not available." << std::endl;
        return;
    }
    auto local_root = local_index->get_root_hash();
    std::cout << "[AntiEntropy] Local Merkle root: " << local_root.to_string() << std::endl;

    // 2. Connect to peer and get their Merkle root
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::ip::tcp::resolver resolver(io_context);
    boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
    std::cout << "[AntiEntropy] Connected to peer " << peer_host_ << ":" << peer_port_ << std::endl;

    // Send command to get peer's Merkle root
    std::string get_root_cmd = "GET_MERKLE_ROOT";
    boost::asio::write(socket, boost::asio::buffer(get_root_cmd));

    char data[4096];
    size_t length = socket.read_some(boost::asio::buffer(data));
    std::string peer_root_str(data, length);
    std::cout << "[AntiEntropy] Peer Merkle root: " << peer_root_str << std::endl;

    // 3. Compare roots
    if (peer_root_str == local_root.to_string()) {
        std::cout << "[AntiEntropy] Merkle roots match. No sync needed." << std::endl;
        return;
    }
    std::cout << "[AntiEntropy] Merkle roots differ. Sync required." << std::endl;

    // 4. Get all keys with timestamps from peer
    std::string get_all_cmd = "GET_ALL";
    boost::asio::write(socket, boost::asio::buffer(get_all_cmd));
    length = socket.read_some(boost::asio::buffer(data));
    std::string peer_keys_str(data, length);
    std::cout << "[AntiEntropy] Peer keys: " << peer_keys_str << std::endl;

    // Parse peer keys
    std::vector<std::string> peer_keys;
    std::istringstream iss(peer_keys_str);
    std::string key_ts;
    while (std::getline(iss, key_ts, ';')) {
        if (!key_ts.empty()) {
            auto pos = key_ts.find(':');
            if (pos != std::string::npos) {
                peer_keys.push_back(key_ts.substr(0, pos));
            }
        }
    }
    std::cout << "[AntiEntropy] Parsed peer keys: ";
    for (const auto& k : peer_keys) std::cout << k << " ";
    std::cout << std::endl;

    // 5. Request Merkle paths for these keys from peer
    std::string get_paths_cmd = "GET_PATHS ";
    for (const auto& k : peer_keys) get_paths_cmd += k + ";";
    boost::asio::write(socket, boost::asio::buffer(get_paths_cmd));
    length = socket.read_some(boost::asio::buffer(data));
    std::string peer_paths_str(data, length);
    std::cout << "[AntiEntropy] Peer paths: " << peer_paths_str << std::endl;

    // Parse peer paths (format: key,hexpath;key,hexpath;...)
    std::vector<std::vector<uint8_t>> peer_paths;
    std::vector<std::string> keys_for_paths;
    std::istringstream pss(peer_paths_str);
    std::string key_path;
    while (std::getline(pss, key_path, ';')) {
        if (!key_path.empty()) {
            auto comma = key_path.find(',');
            if (comma != std::string::npos) {
                keys_for_paths.push_back(key_path.substr(0, comma));
                std::string hexpath = key_path.substr(comma + 1);
                std::vector<uint8_t> path_bytes;
                for (size_t i = 0; i + 1 < hexpath.size(); i += 2) {
                    std::string byte_str = hexpath.substr(i, 2);
                    path_bytes.push_back(static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16)));
                }
                peer_paths.push_back(path_bytes);
            }
        }
    }
    std::cout << "[AntiEntropy] Parsed peer paths for keys: ";
    for (const auto& k : keys_for_paths) std::cout << k << " ";
    std::cout << std::endl;

    // Get local paths for these keys
    auto local_paths = local_index->get_paths(keys_for_paths);

    // Compare paths using MerkleTreeIndex::find_differences
    std::vector<merkle::Path> remote_merkle_paths;
    for (const auto& bytes : peer_paths) {
        remote_merkle_paths.emplace_back(bytes);
    }
    std::vector<std::string> differing_keys = local_index->find_differences(remote_merkle_paths, keys_for_paths);
    std::cout << "[AntiEntropy] Differing keys: ";
    for (const auto& k : differing_keys) std::cout << k << " ";
    std::cout << std::endl;

    // 6. For each differing key, request value from peer and update local store
    for (const auto& key : differing_keys) {
        std::string get_cmd = "GET " + key;
        boost::asio::write(socket, boost::asio::buffer(get_cmd));
        length = socket.read_some(boost::asio::buffer(data));
        std::string value(data, length);
        std::cout << "[AntiEntropy] Updating key '" << key << "' with value '" << value << "'" << std::endl;
        kv_store_.set(key, value, std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    std::cout << "[AntiEntropy] Sync complete." << std::endl;
}
