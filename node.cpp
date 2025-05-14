#include "node.hpp"
#include "anti_entropy/anti_entropy_manager.hpp"
#include <iomanip> // for std::setfill, std::setw, std::hex
#include <boost/asio.hpp>
#include "anti_entropy/merkle_tree_index.hpp"

Node::Node(boost::asio::io_context& io_context, short port, const std::string& peer_host, short peer_port)
    : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      kv_store_(),
      peer_host_(std::move(peer_host)),
      peer_port_(peer_port) {
    start_accept();
}

void Node::start_anti_entropy() {
    std::cout << "start_anti_entropy: entered" << std::endl;
    boost::asio::io_context& io_context = static_cast<boost::asio::io_context&>(acceptor_.get_executor().context());

    auto merkle_index = std::make_shared<MerkleTreeIndex>();
    std::cout << "start_anti_entropy: merkle_index created" << std::endl;
    std::cout << "start_anti_entropy: about to set_merkle_index" << std::endl;
    kv_store_.set_merkle_index(merkle_index);
    std::cout << "start_anti_entropy: merkle_index set in kv_store_" << std::endl;

    anti_entropy_manager_ = std::make_unique<AntiEntropyManager>(
        io_context, kv_store_, peer_host_, peer_port_, merkle_index);
    std::cout << "start_anti_entropy: anti_entropy_manager_ created" << std::endl;

    anti_entropy_manager_->start();
    std::cout << "Started anti-entropy with Merkle tree synchronization" << std::endl;
}

void Node::start_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), this)->start();
            }
            start_accept();
        });
}
