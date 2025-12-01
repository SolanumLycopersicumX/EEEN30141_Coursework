// 11314389_Xu_Task4_Receiver.cpp

#include <iostream>
#include <array>
#include <string>
#define ASIO_STANDALONE 
#include "asio.hpp"

using asio::ip::udp;//UDP协议的好处：无连接、开销小、速度快，适合实时性要求高但允许少量数据丢失的场景，比如比赛更新

int main() {
    try {
        asio::io_context io_context;
        // 1. 绑定监听端口 6000
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 6000));

        std::cout << "=== Task 4 UDP Receiver (Asio) Running on Port 6000 ===" << std::endl;
        std::cout << "Waiting for race updates..." << std::endl;

        std::array<char, 2048> recv_buffer;
        udp::endpoint remote_endpoint;// 用来存“是谁发给我的”

        while (true) {//true是因为要一直接收数据并打印，模拟实时接收比赛更新
            // 2. 阻塞接收，程序运行到这一行会停下来等数据，直到网卡在端口6000收到数据，一旦收到数据，它就把数据放到recv_buffer里，并把发送端的地址信息放到remote_endpoint里，同时返回接收到的数据长度到len变量
            size_t len = socket.receive_from(
                asio::buffer(recv_buffer), remote_endpoint);

            // 3. 转换数据并打印
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