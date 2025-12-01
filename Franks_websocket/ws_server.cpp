// This needs websocketpp library but it also needs the asio library 
//      https://www.zaphoyd.com/projects/websocketpp/
// Add the following lines to tasks.json in the args section
// "-I.",  // This tells the compiler to search in the current directory for tha ASIO files
//"-lws2_32",   // This tells the linker to add the Winsock Windows library
//"-lmswsock",   // linker to add the mswsock library (needed for the server)
// "-D_WIN32_WINNT=0x0601"

#define ASIO_STANDALONE // Do not use Boost
#define _WEBSOCKETPP_NO_BOOST_ // to tell WebSocket++ not to use Boost at all. (needed for WindowsOS)
#define _WEBSOCKETPP_CPP11_THREAD_ // to use C++11 <thread> instead of Boost threads.  (needed for WindowsOS)

#include <memory>   // required for weak_ptr
#include <set>      // required for std::set
#include <asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server;

void my_message_cb(server* pws_server, websocketpp::connection_hdl hdl, server::message_ptr msg)
{
    std::string payload = msg->get_payload();
    std::cout << "Received: " << payload << std::endl;
    if (payload == "exit") {
        pws_server->close(hdl, websocketpp::close::status::normal, "Server closing");
        return;
    }
    try {
        pws_server->send(hdl, "Echo: " + payload, msg->get_opcode()); // Echo message back
    } catch (const websocketpp::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
    }
}

class WebSocketServer {
public:
    WebSocketServer() {
        // Set logging settings
        ws_server.set_access_channels(websocketpp::log::alevel::none);
        ws_server.init_asio();

        // Register handlers
        ws_server.set_open_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << "Client connected." << std::endl;
            connections.insert(hdl);
        });

        ws_server.set_close_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << "Client disconnected." << std::endl;
            connections.erase(hdl);
        });
        ws_server.set_message_handler([&](auto hdl, auto msg){ my_message_cb(&ws_server, hdl, msg); });

        /*
        ws_server.set_message_handler([this](websocketpp::connection_hdl hdl, server::message_ptr msg) {
            std::string payload = msg->get_payload();
            std::cout << "Received: " << payload << std::endl;

            if (payload == "exit") {
                ws_server.close(hdl, websocketpp::close::status::normal, "Server closing");
                return;
            }

            // Echo message back
            ws_server.send(hdl, "Echo: " + payload, msg->get_opcode());
        });*/
    }

    void run(uint16_t port) {
        ws_server.listen(port);
        ws_server.start_accept();
        std::cout << "WebSocket server listening on port " << port << "..." << std::endl;
        ws_server.run();
    }

private:
    server ws_server;
    std::set<
        websocketpp::connection_hdl,
        std::owner_less<websocketpp::connection_hdl>,
        std::allocator<websocketpp::connection_hdl>
    > connections;
    //std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections;
};

int main() {
    try {
        WebSocketServer server;
        server.run(9002);
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
