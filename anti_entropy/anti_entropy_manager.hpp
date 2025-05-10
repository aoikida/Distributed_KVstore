#ifndef ANTI_ENTROPY_MANAGER_HPP
#define ANTI_ENTROPY_MANAGER_HPP

#include <boost/asio.hpp>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include "kv_store.hpp"

using boost::asio::ip::tcp;

class AntiEntropyManager {
public:
    AntiEntropyManager(boost::asio::io_context& io_context, KeyValueStore& kv_store,
                       const std::string& peer_host, short peer_port)
        : io_context_(io_context), kv_store_(kv_store),
          peer_host_(peer_host), peer_port_(peer_port) {}

    void start() {
        anti_entropy_thread_ = std::thread([this]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                run_anti_entropy();
            }
        });
        anti_entropy_thread_.detach();
    }

    void run_anti_entropy() {
        try {
            std::cout << "Running anti-entropy synchronization..." << std::endl;
            
            // Get all keys from peer
            fetch_and_update_all_keys();
            
            // Send all local keys to peer
            auto local_keys = kv_store_.get_all_keys_with_timestamps();
            for (const auto& [key, ts] : local_keys) {
                send_update_to_peer(key);
            }
            
            std::cout << "Anti-entropy synchronization completed." << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Anti-entropy error: " << e.what() << "\n";
        }
    }

private:
    void fetch_and_update_key(const std::string& key) {
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));

            std::string request = "GET " + key;
            boost::asio::write(socket, boost::asio::buffer(request));

            char data[1024];
            size_t length = socket.read_some(boost::asio::buffer(data));
            std::string value(data, length);

            if (!value.empty()) {
                // For simplicity, we'll use the current timestamp
                uint64_t timestamp = current_timestamp();
                kv_store_.set(key, value, timestamp);
                std::cout << "Updated key from peer: " << key << " = " << value << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << "Failed to fetch and update key " << key << ": " << e.what() << "\n";
        }
    }

    void fetch_and_update_all_keys() {
        try {
            tcp::socket socket(io_context_);
            tcp::resolver resolver(io_context_);
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));

            std::string request = "GET_ALL";
            boost::asio::write(socket, boost::asio::buffer(request));

            char data[1024];
            size_t length = socket.read_some(boost::asio::buffer(data));
            std::string response(data, length);

            std::unordered_map<std::string, uint64_t> keys_with_timestamps;
            parse_keys_with_timestamps(response, keys_with_timestamps);

            for (const auto& kv : keys_with_timestamps) {
                fetch_and_update_key(kv.first);
            }
        } catch (std::exception& e) {
            std::cerr << "Failed to fetch and update all keys: " << e.what() << "\n";
        }
    }

    void parse_keys_with_timestamps(const std::string& data, std::unordered_map<std::string, uint64_t>& out_map) {
        if (data.empty()) return;
        
        // Simple parsing assuming format: key1:timestamp1;key2:timestamp2;...
        size_t start = 0;
        while (start < data.size()) {
            size_t sep = data.find(':', start);
            if (sep == std::string::npos) break;
            std::string key = data.substr(start, sep - start);
            size_t end = data.find(';', sep);
            std::string ts_str = (end == std::string::npos) ? data.substr(sep + 1) : data.substr(sep + 1, end - sep - 1);
            uint64_t ts = std::stoull(ts_str);
            out_map[key] = ts;
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
            std::cerr << "Failed to send update to peer for key " << key << ": " << e.what() << "\n";
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
                std::cerr << "Failed to propagate update: " << e.what() << "\n";
            }
        }
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
    std::thread anti_entropy_thread_;
};

#endif // ANTI_ENTROPY_MANAGER_HPP
