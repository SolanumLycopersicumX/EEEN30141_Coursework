// 11314389_Xu_Task4_Receiver.cpp

#include <iostream>
#include <array>
#include <string>
#define ASIO_STANDALONE 
#include "asio.hpp"

using asio::ip::udp;

int main() {
    try {
        asio::io_context io_context;
        // 监听 6000 端口
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 6000));

        std::cout << "=== Task 4 UDP Receiver (Asio) Running on Port 6000 ===" << std::endl;
        std::cout << "Waiting for race updates..." << std::endl;

        std::array<char, 2048> recv_buffer;
        udp::endpoint remote_endpoint;

        while (true) {
            size_t len = socket.receive_from(
                asio::buffer(recv_buffer), remote_endpoint);

            // 转换数据并打印
            std::string msg(recv_buffer.data(), len);
            
            // 【关键】使用 << std::flush 强制让文字立即跳出来
            std::cout << msg << std::flush;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Receiver Exception: " << e.what() << std::endl;
    }

    return 0;
}