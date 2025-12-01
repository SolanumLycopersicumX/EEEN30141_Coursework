// 11314389_Xu_Task5_Benchmark_Strict.cpp
// Task 5: Latency Benchmark (Strict RTT & Ping-Pong)
// Mode: Round-Trip Time (Request -> Process -> Response)

#define ASIO_STANDALONE
#define _WEBSOCKETPP_NO_BOOST_
#define _WEBSOCKETPP_CPP11_THREAD_

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <iomanip>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Franks_websocket/asio.hpp"
#include "Franks_websocket/websocketpp/config/asio_no_tls.hpp"
#include "Franks_websocket/websocketpp/config/asio_no_tls_client.hpp"
#include "Franks_websocket/websocketpp/server.hpp"
#include "Franks_websocket/websocketpp/client.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

using namespace std;
using namespace std::chrono;

const int ITERATIONS = 10000;
const string TEST_MSG = "Ping"; // 保持简短以聚焦于延迟而非带宽
const unsigned short WEBSOCKET_PORT = 9002;
const string WEBSOCKET_URI = "ws://127.0.0.1:9002";

// --- 1. Thread Ping-Pong Environment ---
// 只要涉及到线程间的严格交替，必须使用条件变量 (Condition Variable)
mutex pp_mtx;
condition_variable cv_ping, cv_pong;
bool ping_ready = false; // 主线程通知子线程
bool pong_ready = false; // 子线程通知主线程
bool stop_thread = false;

// 乒乓线程：等待 Ping，发送 Pong
void ping_pong_worker() {
    unique_lock<mutex> lock(pp_mtx);
    while (true) {
        // 等待主线程发出 Ping 信号
        cv_ping.wait(lock, [] { return ping_ready || stop_thread; });
        
        if (stop_thread) break;

        // 模拟处理：消费 Ping 信号
        ping_ready = false;

        // 发出 Pong 信号
        pong_ready = true;
        lock.unlock(); // 必须先解锁，才能让主线程尽快获取锁
        cv_pong.notify_one();
        
        lock.lock(); // 重新加锁等待下一轮
    }
}

// --- 2. UDP Echo Environment ---
void udp_echo_server_thread() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in serverAddr, clientAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(6000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 设置超时防止死锁
    DWORD timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    bind(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    char buf[1024];
    int clientAddrLen = sizeof(clientAddr);
    
    // 简单的回显循环：收到什么发回什么
    for (int i = 0; i < ITERATIONS; ++i) {
        int len = recvfrom(sock, buf, 1024, 0, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (len > 0) {
            sendto(sock, buf, len, 0, (SOCKADDR*)&clientAddr, clientAddrLen);
        }
    }
    closesocket(sock);
}

// --- 3. WebSocket Echo (for Strict RTT Benchmark) ---
class WebSocketEchoServer {
    using server_type = websocketpp::server<websocketpp::config::asio>;

public:
    explicit WebSocketEchoServer(unsigned short port) : port_(port) {
        endpoint_.init_asio();
        endpoint_.set_reuse_addr(true);
        endpoint_.set_access_channels(websocketpp::log::alevel::none);
        endpoint_.set_message_handler(
            [this](websocketpp::connection_hdl hdl, server_type::message_ptr msg) {
                try {
                    endpoint_.send(hdl, msg->get_payload(), msg->get_opcode());
                } catch (const websocketpp::exception& e) {
                    cerr << "WebSocket server send error: " << e.what() << endl;
                }
            }
        );
    }

    void run() {
        try {
            endpoint_.listen(port_);
            endpoint_.start_accept();
            endpoint_.run();
        } catch (const std::exception& e) {
            cerr << "WebSocket server error: " << e.what() << endl;
        }
    }

    void stop() {
        websocketpp::lib::error_code ec;
        endpoint_.stop_listening(ec);
        endpoint_.stop();
    }

private:
    server_type endpoint_;
    unsigned short port_;
};

class WebSocketRTTClient {
    using client_type = websocketpp::client<websocketpp::config::asio_client>;

public:
    WebSocketRTTClient() {
        client_.set_access_channels(websocketpp::log::alevel::none);
        client_.init_asio();

        client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            {
                lock_guard<mutex> lock(mtx_);
                hdl_ = hdl;
                connected_ = true;
            }
            cv_connected_.notify_one();
        });

        client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            client_type::connection_ptr con = client_.get_con_from_hdl(hdl);
            {
                lock_guard<mutex> lock(mtx_);
                has_error_ = true;
                error_msg_ = con ? con->get_ec().message() : "unknown error";
            }
            cv_connected_.notify_one();
            cv_message_.notify_all();
        });

        client_.set_message_handler([this](websocketpp::connection_hdl, client_type::message_ptr) {
            {
                lock_guard<mutex> lock(mtx_);
                message_received_ = true;
            }
            cv_message_.notify_one();
        });
    }

    void connect(const string& uri) {
        websocketpp::lib::error_code ec;
        client_type::connection_ptr con = client_.get_connection(uri, ec);
        if (ec) {
            throw runtime_error("WebSocket connection creation failed: " + ec.message());
        }

        client_.connect(con);
        client_thread_ = thread([this]() { client_.run(); });

        unique_lock<mutex> lock(mtx_);
        cv_connected_.wait(lock, [this]() { return connected_ || has_error_; });
        if (has_error_ && !connected_) {
            throw runtime_error("WebSocket connection failed: " + error_msg_);
        }
    }

    void send_and_wait(const string& payload) {
        {
            lock_guard<mutex> lock(mtx_);
            if (has_error_) {
                throw runtime_error("WebSocket connection error: " + error_msg_);
            }
            message_received_ = false;
        }

        try {
            client_.send(hdl_, payload, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            throw runtime_error("WebSocket send failed: " + string(e.what()));
        }

        unique_lock<mutex> lock(mtx_);
        cv_message_.wait(lock, [this]() { return message_received_ || has_error_; });
        if (has_error_) {
            throw runtime_error("WebSocket receive failed: " + error_msg_);
        }
    }

    void close() {
        if (closed_) return;

        websocketpp::lib::error_code ec;
        if (connected_) {
            client_.close(hdl_, websocketpp::close::status::going_away, "benchmark complete", ec);
            connected_ = false;
        }

        client_.stop();
        if (client_thread_.joinable()) {
            client_thread_.join();
        }
        closed_ = true;
    }

    ~WebSocketRTTClient() {
        try {
            close();
        } catch (...) {
            // destructor should not throw
        }
    }

private:
    client_type client_;
    websocketpp::connection_hdl hdl_;
    thread client_thread_;
    mutex mtx_;
    condition_variable cv_connected_;
    condition_variable cv_message_;
    bool connected_ = false;
    bool message_received_ = false;
    bool has_error_ = false;
    string error_msg_;
    bool closed_ = false;
};

void init_winsock() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

int main() {
    init_winsock();

    cout << "=== Task 5: Strict Latency Benchmark (RTT) ===" << endl;
    cout << "Mode: Request-Response (Blocking Wait)" << endl;
    cout << "Iterations: " << ITERATIONS << endl << endl;

    // -------------------------------------------------
    // 1. Thread Ping-Pong Test
    // -------------------------------------------------
    // 这里的关键是测试条件变量唤醒和上下文切换的开销
    thread worker(ping_pong_worker);
    
    // 预热 (可选，防止第一次分配延迟)
    this_thread::sleep_for(milliseconds(10));

    auto start_thread = high_resolution_clock::now();
    
    {
        unique_lock<mutex> lock(pp_mtx);
        for (int i = 0; i < ITERATIONS; ++i) {
            // Step 1: 触发 Ping
            ping_ready = true;
            cv_ping.notify_one();
            
            // Step 2: 等待 Pong (阻塞等待)
            cv_pong.wait(lock, [] { return pong_ready; });
            
            // Step 3: 重置状态，准备下一轮
            pong_ready = false;
        }
        // 测试结束，设置退出标志
        stop_thread = true;
        cv_ping.notify_one();
    }
    
    auto end_thread = high_resolution_clock::now();
    worker.join();

    double total_thread = (double)duration_cast<microseconds>(end_thread - start_thread).count();
    double avg_thread = total_thread / ITERATIONS;

    // -------------------------------------------------
    // 2. UDP End-to-End RTT Test
    // -------------------------------------------------
    thread udpServer(udp_echo_server_thread);
    this_thread::sleep_for(milliseconds(100)); // 让服务器先跑起来

    SOCKET udpClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in udpDestAddr;
    udpDestAddr.sin_family = AF_INET;
    udpDestAddr.sin_port = htons(6000);
    udpDestAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 同样设置超时
    DWORD udp_timeout = 1000;
    setsockopt(udpClient, SOL_SOCKET, SO_RCVTIMEO, (const char*)&udp_timeout, sizeof(udp_timeout));

    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    char udpBuf[1024];

    auto start_udp = high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        // Step 1: Send
        sendto(udpClient, TEST_MSG.c_str(), TEST_MSG.length(), 0, (SOCKADDR*)&udpDestAddr, sizeof(udpDestAddr));
        
        // Step 2: Blocking Receive (Wait for Echo)
        // 只有收到了，才算这轮结束
        recvfrom(udpClient, udpBuf, 1024, 0, (SOCKADDR*)&fromAddr, &fromLen);
    }
    auto end_udp = high_resolution_clock::now();
    
    closesocket(udpClient);
    if(udpServer.joinable()) udpServer.join();

    double total_udp = (double)duration_cast<microseconds>(end_udp - start_udp).count();
    double avg_udp = total_udp / ITERATIONS;

    // -------------------------------------------------
    // 3. WebSocket Strict RTT Test
    // -------------------------------------------------
    double total_ws = 0.0;
    double avg_ws = 0.0;

    WebSocketEchoServer wsServer(WEBSOCKET_PORT);
    thread wsServerThread([&wsServer]() { wsServer.run(); });

    auto cleanup_ws = [&wsServer, &wsServerThread]() {
        wsServer.stop();
        if (wsServerThread.joinable()) {
            wsServerThread.join();
        }
    };

    try {
        this_thread::sleep_for(milliseconds(200)); // Allow server to start

        WebSocketRTTClient wsClient;
        wsClient.connect(WEBSOCKET_URI);

        auto start_ws = high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) {
            wsClient.send_and_wait(TEST_MSG);
        }
        auto end_ws = high_resolution_clock::now();

        total_ws = (double)duration_cast<microseconds>(end_ws - start_ws).count();
        avg_ws = total_ws / ITERATIONS;

        wsClient.close();
        cleanup_ws();
    } catch (const std::exception& e) {
        cleanup_ws();
        cerr << "WebSocket RTT benchmark failed: " << e.what() << endl;
        WSACleanup();
        return 1;
    }

    // -------------------------------------------------
    // REPORT
    // -------------------------------------------------
    cout << endl << string(70, '=') << endl;
    cout << " FINAL LATENCY RESULTS (Average RTT per operation)" << endl;
    cout << string(70, '=') << endl;
    cout << left << setw(25) << "Method" 
         << setw(15) << "Total(us)" 
         << setw(15) << "Avg RTT(us)" 
         << "Comparison" << endl;
    cout << string(70, '-') << endl;

    // 1. Threads
    cout << left << setw(25) << "Thread Ping-Pong" 
         << setw(15) << total_thread 
         << setw(15) << avg_thread 
         << "1.00x (Baseline)" << endl;

    // 2. UDP
    double ratio_udp = avg_udp / avg_thread;
    cout << left << setw(25) << "UDP End-to-End RTT" 
         << setw(15) << total_udp 
         << setw(15) << avg_udp 
         << fixed << setprecision(2) << ratio_udp << "x Slower" << endl;

    // 3. WebSocket
    double ratio_ws = avg_ws / avg_thread;
    cout << left << setw(25) << "WebSocket Strict RTT" 
         << setw(15) << total_ws 
         << setw(15) << avg_ws 
         << fixed << setprecision(2) << ratio_ws << "x Slower" << endl;
         
    cout << string(70, '=') << endl << endl;

    WSACleanup();
    return 0;
}
