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

// You can open multiple clients to attach to a single websocket server.

#include <asio.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <thread>

typedef websocketpp::client<websocketpp::config::asio_client> client;

// Forward declaration of handler function
void on_open(client* ws_client, websocketpp::connection_hdl hdl);

// Message handler
void my_message_cb(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg) {
    std::cout << "Received from server: " << msg->get_payload() << std::endl;
}

// Open handler implementation
void on_open(client* ws_client, websocketpp::connection_hdl hdl) {
    std::cout << "Connected to server!" << std::endl;

    // Start a detached thread to handle console input
    std::thread input_thread([ws_client, hdl]() {
    std::cout << "Enter messages to send to the server. Type 'exit' to finish" << std::endl;
        for (;;) {
            std::string msg;
            std::getline(std::cin, msg);

            if (msg == "exit") {
                ws_client->close(hdl, websocketpp::close::status::normal, "Client exiting");
                break;
            }

            ws_client->send(hdl, msg, websocketpp::frame::opcode::text);
        }
    });
    input_thread.detach();
}

int main() {
    try {
        client ws_client;
        ws_client.set_access_channels(websocketpp::log::alevel::none);
        ws_client.init_asio();

        // Register function-based handlers
        ws_client.set_open_handler(
            websocketpp::lib::bind(&on_open, &ws_client, websocketpp::lib::placeholders::_1)
        );

        ws_client.set_message_handler(
            websocketpp::lib::bind(&my_message_cb, &ws_client,
                                   websocketpp::lib::placeholders::_1,
                                   websocketpp::lib::placeholders::_2)
        );

        ws_client.set_fail_handler([&](websocketpp::connection_hdl hdl) {
            client::connection_ptr con = ws_client.get_con_from_hdl(hdl);
            std::cout << "Connection failed: " << con->get_ec().message() << std::endl;
        });

        std::string uri = "ws://127.0.0.1:9002";
        websocketpp::lib::error_code ec;
        client::connection_ptr con = ws_client.get_connection(uri, ec);
        if (ec) {
            std::cerr << "Connection failed: " << ec.message() << std::endl;
            return 1;
        }

        ws_client.connect(con);
        ws_client.run();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}