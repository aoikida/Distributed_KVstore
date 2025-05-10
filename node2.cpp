#include "node.hpp"
#include "anti_entropy/anti_entropy_manager.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;
        Node node(io_context, 5009, "127.0.0.1", 5008);
        node.start_anti_entropy();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
