#ifndef NODE_HPP
#define NODE_HPP

#include "kv_store.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <memory>
#include <array>
#include <unordered_map>
#include <sstream>
#include <chrono>

using boost::asio::ip::tcp;

// Forward declaration
class AntiEntropyManager;

class Node {
public:
    Node(boost::asio::io_context& io_context,
         short port,
         const std::string& peer_host = "",
         short peer_port = 0);
         
    // Session class for handling client connections
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, KeyValueStore& kv_store)
            : socket_(std::move(socket)), kv_store_(kv_store) {}
        
        void start() {
            do_read();
        }
        
    private:
        void do_read() {
            auto self(shared_from_this());
            socket_.async_read_some(boost::asio::buffer(data_),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        std::string request(data_.data(), length);
                        std::string response = kv_store_.process_command(request);
                        do_write(response);
                    }
                });
        }
        
        void do_write(const std::string& response) {
            auto self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(response),
                [this, self](boost::system::error_code ec, std::size_t) {
                    if (!ec) {
                        // Connection closed after write
                    }
                });
        }
        
        tcp::socket socket_;
        KeyValueStore& kv_store_;
        std::array<char, 1024> data_;
    };

    // Start accepting client connections
    void start_accept();
    
    // Start the anti-entropy synchronization process
    void start_anti_entropy();

    std::string process_command(const std::string& command) {
        std::istringstream iss(command);
        std::string first, action, key, value;
        iss >> first;

        bool is_propagated = false;
        if (first == "PROPAGATE") {
            is_propagated = true;
            iss >> action >> key >> value;
        } else {
            action = first;
            iss >> key >> value;
        }

        if (action == "GET") {
            return kv_store_.get(key);
        } else if (action == "SET") {
            uint64_t timestamp = current_timestamp();
            kv_store_.set(key, value, timestamp);
            if (!is_propagated) propagate_update("PROPAGATE SET " + key + " " + value + " " + std::to_string(timestamp));
            return "OK";
        } else if (action == "DEL") {
            uint64_t timestamp = current_timestamp();
            kv_store_.del(key, timestamp);
            if (!is_propagated) propagate_update("PROPAGATE DEL " + key + " " + value + " " + std::to_string(timestamp));
            return "OK";
        }
        return "Invalid command";
    }

    void propagate_update(const std::string& command) {
        if (!peer_host_.empty() && peer_port_ > 0) {
            std::thread([this, command]() {
                int retry_count = 0;
                const int max_retries = 5;
                const int initial_delay = 100; // milliseconds
                
                while (retry_count < max_retries) {
                    try {
                        int delay = initial_delay * (1 << retry_count); // exponential backoff
                        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                        
                        tcp::socket socket(acceptor_.get_executor());
                        tcp::resolver resolver(acceptor_.get_executor());
                        boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));
                        boost::asio::write(socket, boost::asio::buffer(command));
                        return; // Success, exit the retry loop
                    } catch (std::exception& e) {
                        std::cerr << "Failed to propagate update (attempt " 
                                  << (retry_count + 1) << "): " << e.what() << "\n";
                        retry_count++;
                    }
                }
                std::cerr << "Failed to propagate update after " << max_retries << " attempts\n";
            }).detach();
        }
    }

private:
    std::unique_ptr<AntiEntropyManager> anti_entropy_manager_;

    uint64_t current_timestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    void parse_keys_with_timestamps(const std::string& data, std::unordered_map<std::string, uint64_t>& out_map) {
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

    void fetch_and_update_key(const std::string& key) {
        try {
            tcp::socket socket(acceptor_.get_executor());
            tcp::resolver resolver(acceptor_.get_executor());
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));

            std::string request = "GET " + key;
            boost::asio::write(socket, boost::asio::buffer(request));

            char data[1024];
            size_t length = socket.read_some(boost::asio::buffer(data));
            std::string value(data, length);

            // For simplicity, assume value does not contain timestamp; set with current time
            uint64_t timestamp = current_timestamp();
            kv_store_.set(key, value, timestamp);
        } catch (std::exception& e) {
            std::cerr << "Failed to fetch and update key " << key << ": " << e.what() << "\n";
        }
    }
    
    void fetch_and_update_all_keys() {
        try {
            tcp::socket socket(acceptor_.get_executor());
            tcp::resolver resolver(acceptor_.get_executor());
            boost::asio::connect(socket, resolver.resolve(peer_host_, std::to_string(peer_port_)));

            std::string request = "GET ALL";
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

    void send_update_to_peer(const std::string& key) {
        try {
            auto val_ts = kv_store_.get_value_with_timestamp(key);
            std::string command = "PROPAGATE SET " + key + " " + val_ts.value + " " + std::to_string(val_ts.timestamp);
            propagate_update(command);
        } catch (std::exception& e) {
            std::cerr << "Failed to send update to peer for key " << key << ": " << e.what() << "\n";
        }
    }

    tcp::acceptor acceptor_;
    KeyValueStore kv_store_;
    std::string peer_host_;
    short peer_port_;
};

#endif // NODE_HPP
