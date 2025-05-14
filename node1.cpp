#include "node.hpp"
#include "anti_entropy/anti_entropy_manager.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        std::cout << "main() started" << std::endl;
        boost::asio::io_context io_context;
        std::cout << "io_context created" << std::endl;
        Node node(io_context, 5008, "127.0.0.1", 5009);
        std::cout << "Node created" << std::endl;
        node.start_anti_entropy();
        std::cout << "Started anti-entropy" << std::endl;
        std::cout << "About to run io_context" << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
