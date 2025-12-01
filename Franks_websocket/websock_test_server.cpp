// This needs the websocketpp library but it also needs the asio library for some boost headers
// Add the following line to tasks.json in the args section
//     "-I.", // include current directory in the include search path

#define ASIO_STANDALONE // Do not use Boost
#include <asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>

using server = websocketpp::server<websocketpp::config::asio>;

void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::cout << "Received: " << msg->get_payload() << std::endl;
    try {
        s->send(hdl, msg->get_payload(), msg->get_opcode()); // echo
    } catch (const websocketpp::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
    }
}

int main() {
    server srv;
    try {
        // Optional logging
        srv.set_access_channels(websocketpp::log::alevel::all);
        srv.clear_access_channels(websocketpp::log::alevel::frame_payload);

        srv.init_asio(); // standalone Asio
        srv.set_message_handler([&](auto hdl, auto msg){ on_message(&srv, hdl, msg); });

        srv.listen(9002);
        srv.start_accept();

        std::cout << "WebSocket++ echo server listening on ws://localhost:9002\n";
        srv.run();
    } catch (const std::exception& e) {
        std::cerr << "Server exception: " << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cerr << "WebSocket++ error: " << e.message() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error\n";
    }
    return 0;
}