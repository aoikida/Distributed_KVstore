#ifndef ANTI_ENTROPY_MANAGER_HPP
#define ANTI_ENTROPY_MANAGER_HPP

#include <boost/asio.hpp>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include <chrono>
#include <memory>
#include "index_interface.hpp"

// Forward declarations
class KeyValueStore;

using boost::asio::ip::tcp;

class AntiEntropyManager {
public:
    enum SyncMode {
        FULL_STATE_EXCHANGE,
        MERKLE_TREE
    };

    AntiEntropyManager(boost::asio::io_context& io_context, KeyValueStore& kv_store,
                       const std::string& peer_host, short peer_port,
                       std::shared_ptr<IndexInterface> merkle_index,
                       SyncMode mode = MERKLE_TREE);

    void start();
    void run_anti_entropy();
    std::shared_ptr<IndexInterface> get_merkle_index() const { return merkle_index_; }
    void set_merkle_index(std::shared_ptr<IndexInterface> index) { merkle_index_ = index; }

private:
    // ... (keep existing private method declarations but remove implementations)
    
    // Private member variables
    boost::asio::io_context& io_context_;
    KeyValueStore& kv_store_;
    std::string peer_host_;
    short peer_port_;
    SyncMode sync_mode_;
    std::shared_ptr<IndexInterface> merkle_index_;
    std::thread anti_entropy_thread_;
};

#endif // ANTI_ENTROPY_MANAGER_HPP
