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
condition_variable cv_ping, cv_pong;//作用：信号枪-用来唤醒主线程和子线程
//bool用来防止虚假唤醒，确保线程只在正确的条件下继续执行
bool ping_ready = false; // 主线程通知子线程“球打过去了”
bool pong_ready = false; // 子线程通知主线程“球打回来了”
bool stop_thread = false;// 停止线程标志

// 乒乓线程：等待 Ping，发送 Pong
void ping_pong_worker() {
    unique_lock<mutex> lock(pp_mtx);
    while (true) {
        // 等待主线程发出 Ping 信号
        cv_ping.wait(lock, [] { return ping_ready || stop_thread; });
        
        //检查是否需要退出线程
        if (stop_thread) break;

        // 处理：消费 Ping 信号
        ping_ready = false;

        // 生产：发出 Pong 信号
        pong_ready = true;
        
        //手动解锁并通知主线程
        lock.unlock(); // 必须先解锁，才能让主线程尽快获取锁
        cv_pong.notify_one();
        
        lock.lock(); // 重新加锁等待下一轮
    }
}

// --- 2. UDP Echo Environment ---
void udp_echo_server_thread() {
    // [1] 创建UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//这里是sock_dgram表示UDP协议,如果是TCP协议就要用sock_stream
    // [2] 配置服务器地址
    sockaddr_in serverAddr, clientAddr;
    serverAddr.sin_family = AF_INET;// IPv4 协议
    serverAddr.sin_port = htons(6000);// 监听端口 6000
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");// 本机地址
    
    // [3] 设置超时 (防止线程死锁无法退出)
    DWORD timeout = 1000;// 1000 毫秒 = 1 秒
    //如果超时，就让 recvfrom 函数返回错误，避免无限期阻塞（死锁线程无法退出）
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    // [4] 绑定端口 (开门营业)（bind占据端口，只有服务器端需要bind。但凡发往6000的数据包全部接收）
    bind(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    char buf[1024];
    int clientAddrLen = sizeof(clientAddr);
    
    // [5] 简单的回显循环：收到什么发回什么
    for (int i = 0; i < ITERATIONS; ++i) {
        // [6] 接收数据 (阻塞等待)，同时recvfrom会把发送端地址存到clientAddr里（记录是谁发给我的），同时返回接收到的数据长度并存到len变量
        int len = recvfrom(sock, buf, 1024, 0, (SOCKADDR*)&clientAddr, &clientAddrLen);
        // [7] 发送回数据 (Echo)
        if (len > 0) {
            sendto(sock, buf, len, 0, (SOCKADDR*)&clientAddr, clientAddrLen);
        }
    }
    closesocket(sock);
}

// --- 3. WebSocket Echo (for Strict RTT Benchmark) ------之所以慢是因为有TCP handshake and confirmation，framing（给数据加header），masking（数据加密）
class WebSocketEchoServer {
    // 定义服务器类型，基于 Asio
    using server_type = websocketpp::server<websocketpp::config::asio>;

public:
    explicit WebSocketEchoServer(unsigned short port) : port_(port) {
        // [1] 初始化 Asio 系统
        endpoint_.init_asio();
        // [2] 允许地址重用 (避免重启程序时报 "Port already in use")
        endpoint_.set_reuse_addr(true);
        // [3] 关闭日志 (关键!)
        // 如果不关日志，大量的控制台输出会严重拖慢速度，导致测量结果不准确。
        endpoint_.set_access_channels(websocketpp::log::alevel::none);
        // [4] 设置消息处理器 (The "Echo" Logic)
        endpoint_.set_message_handler(
            [this](websocketpp::connection_hdl hdl, server_type::message_ptr msg) {
                // Lambda 函数：当收到消息时被调用
                try {
                    // // msg->get_payload(): 获取消息内容 (例如 "Ping")
                    // // msg->get_opcode(): 获取消息类型 (文本/二进制)
                    // endpoint_.send: 把同样的内容和类型发回去
                    endpoint_.send(hdl, msg->get_payload(), msg->get_opcode());
                } catch (const websocketpp::exception& e) {
                    cerr << "WebSocket server send error: " << e.what() << endl;
                }
            }
        );
    }

    void run() {
        // 标准启动流程
        try {
            endpoint_.listen(port_);// 监听端口
            endpoint_.start_accept();// 开始接受连接
            endpoint_.run();// 进入事件循环 (Blocking)
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
        // [1] 关闭日志，初始化 Asio
        client_.set_access_channels(websocketpp::log::alevel::none);
        client_.init_asio();
        // [2] 设置连接成功的回调 (On Open)
        client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            {
                lock_guard<mutex> lock(mtx_);
                hdl_ = hdl;// 保存连接句柄，以便后续发送数据用
                connected_ = true;// 标记：连上了
            }
            cv_connected_.notify_one();// 唤醒正在 connect() 函数里等待的主线程
        });

        // [3] 设置收到消息的回调 (On Message)
        client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            client_type::connection_ptr con = client_.get_con_from_hdl(hdl);
            {
                lock_guard<mutex> lock(mtx_);
                has_error_ = true;// 标记：收到回显了
                error_msg_ = con ? con->get_ec().message() : "unknown error";
            }
            cv_connected_.notify_one();// 唤醒正在 send_and_wait() 函数里等待的主线程
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
        // 创建连接请求
        client_type::connection_ptr con = client_.get_connection(uri, ec);
        if (ec) {
            throw runtime_error("WebSocket connection creation failed: " + ec.message());
        }

        client_.connect(con);
        // [关键点] 启动后台线程跑网络 IO
        // client_.run() 是个死循环，必须放在新线程里，否则主线程会卡死在这里
        client_thread_ = thread([this]() { client_.run(); });

        // [同步等待]
        // 主线程必须停在这里，直到 set_open_handler 被触发
        unique_lock<mutex> lock(mtx_);
        cv_connected_.wait(lock, [this]() { return connected_ || has_error_; });
        if (has_error_ && !connected_) {
            throw runtime_error("WebSocket connection failed: " + error_msg_);
        }
    }

    void send_and_wait(const string& payload) {
        {
            // [1] 发送前重置标志位
            lock_guard<mutex> lock(mtx_);
            if (has_error_) {
                throw runtime_error("WebSocket connection error: " + error_msg_);
            }
            // "我还没收到回信"
            message_received_ = false;
        }

        try {
            // [2] 非阻塞发送
            // 这行代码执行极快，只是把数据放入库的发送队列，函数立即返回
            client_.send(hdl_, payload, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            throw runtime_error("WebSocket send failed: " + string(e.what()));
        }

        // [3] 强制阻塞 (Force Blocking)
        // 这一步把“异步发送”变成了“同步等待”
        unique_lock<mutex> lock(mtx_);
        cv_message_.wait(lock, [this]() { return message_received_ || has_error_; });
        // 当 wait 返回时，说明 set_message_handler 被触发了，也就意味着数据跑了一个来回。
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
    
    //启动子线程
    thread worker(ping_pong_worker);
    
    // 预热 (可选，防止第一次分配延迟)
    this_thread::sleep_for(milliseconds(10));

    auto start_thread = high_resolution_clock::now();
    
    {
        unique_lock<mutex> lock(pp_mtx);//主线程先获取锁

        for (int i = 0; i < ITERATIONS; ++i) {
            // Step 1: 触发 Ping，发球
            ping_ready = true;
            cv_ping.notify_one();
            
            // Step 2: 等待 Pong (阻塞等待)，等待球回来，同时释放锁让子线程运行
            cv_pong.wait(lock, [] { return pong_ready; });
            
            // Step 3: 重置状态，准备下一轮
            pong_ready = false;
        }
        // 测试结束，设置退出标志
        stop_thread = true;
        //唤醒子线程以便其检查stop_thread并退出
        cv_ping.notify_one();
    }
    
    auto end_thread = high_resolution_clock::now();
    worker.join();//等待子线程结束

    double total_thread = (double)duration_cast<microseconds>(end_thread - start_thread).count();
    double avg_thread = total_thread / ITERATIONS;

    // -------------------------------------------------
    // 2. UDP End-to-End RTT Test
    // -------------------------------------------------
    thread udpServer(udp_echo_server_thread);
    this_thread::sleep_for(milliseconds(100)); // 让服务器先跑起来

    //创建客户端 UDP Socket
    SOCKET udpClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // 准备目标地址（我要发给谁）
    sockaddr_in udpDestAddr;
    udpDestAddr.sin_family = AF_INET;
    udpDestAddr.sin_port = htons(6000);
    udpDestAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 同样设置接收超时
    DWORD udp_timeout = 1000;
    setsockopt(udpClient, SOL_SOCKET, SO_RCVTIMEO, (const char*)&udp_timeout, sizeof(udp_timeout));

    //存储回信者的地址
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    char udpBuf[1024];

    //开始计时
    auto start_udp = high_resolution_clock::now();

    //测量循环并计算 RTT-Round Trip Time
    for (int i = 0; i < ITERATIONS; ++i) {
        // Step 1: Send发送ping
        sendto(udpClient, TEST_MSG.c_str(), TEST_MSG.length(), 0, (SOCKADDR*)&udpDestAddr, sizeof(udpDestAddr));
        
        // Step 2: Blocking Receive (Wait for Echo)等待pong
        // 只有收到了，才算这轮结束，否则继续等，如果没有这一行，程序会一直发送sendto而不等待回应
        recvfrom(udpClient, udpBuf, 1024, 0, (SOCKADDR*)&fromAddr, &fromLen);
    }
    //结束计时
    auto end_udp = high_resolution_clock::now();
    
    closesocket(udpClient);
    if(udpServer.joinable()) udpServer.join();

    double total_udp = (double)duration_cast<microseconds>(end_udp - start_udp).count();
    double avg_udp = total_udp / ITERATIONS;

    // -------------------------------------------------
    // 3. WebSocket Strict RTT Test
    //之所以慢是因为有TCP handshake and confirmation，framing（给数据加header），masking（数据加密）
    //至少三个线程同时再跑：主线程（负责循环和计时），Client IO线程（负责处理底层TCP数据并回调on_message），Server线程（运行服务器的endpoint_.run()）负责echo
    //涉及三个线程的调度和切换，加上锁的竞争，所以 WebSocket 的 RTT 波动通常比单线程 UDP 更大
    //Main Thread (send发送) -> Client Thread (Pack & Encrypt打包加密) -> Network -> Server Thread (Decrypt & Echo解密回显) -> Network -> Client Thread (Receive & Notify接收通知) -> Main Thread (wait 唤醒)。
    // -------------------------------------------------
    double total_ws = 0.0;
    double avg_ws = 0.0;

    // 启动服务器线程
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

        // 初始化客户端并连接
        WebSocketRTTClient wsClient;
        wsClient.connect(WEBSOCKET_URI);

        auto start_ws = high_resolution_clock::now();
        // [测量循环]
        for (int i = 0; i < ITERATIONS; ++i) {
            // 这行代码包含了：发送 -> 等待 -> 接收 -> 唤醒
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
