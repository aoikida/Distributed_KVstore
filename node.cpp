#include "node.hpp"
#include "anti_entropy/anti_entropy_manager.hpp"
#include <iomanip> // for std::setfill, std::setw, std::hex
#include <boost/asio.hpp>

Node::Node(boost::asio::io_context& io_context, short port, const std::string& peer_host, short peer_port)
    : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      kv_store_(),
      peer_host_(std::move(peer_host)),
      peer_port_(peer_port) {
    start_accept();
}

void Node::start_anti_entropy() {
    boost::asio::io_context& io_context = static_cast<boost::asio::io_context&>(acceptor_.get_executor().context());
    
    // Create the anti-entropy manager with Merkle tree synchronization enabled
    anti_entropy_manager_ = std::make_unique<AntiEntropyManager>(
        io_context, kv_store_, peer_host_, peer_port_);
    
    anti_entropy_manager_->start();
    std::cout << "Started anti-entropy with Merkle tree synchronization" << std::endl;
}

void Node::start_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), kv_store_)->start();
            }
            start_accept();
        });
}
