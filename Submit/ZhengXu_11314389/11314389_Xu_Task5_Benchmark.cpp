// 11314389_Xu_Task5_Benchmark.cpp
// Task 5: Performance Benchmark (Threads vs UDP)

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace std::chrono;

const int ITERATIONS = 10000; // 测试次数
const string TEST_MSG = "Benchmark Data Packet"; // 测试发送的数据

// --- 1. 线程通信测试环境 ---
mutex mtx;
string shared_data; // 模拟共享内存
void thread_receiver() {
    // 这里仅仅为了模拟存在一个接收线程，不做实际复杂操作
    // 在真实场景中，会有锁竞争
}

// --- 2. UDP 通信测试环境 ---
sockaddr_in destAddr;
SOCKET sendSocket;

void setup_udp() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(9999); // 随便找个端口
    destAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
}

void cleanup_udp() {
    closesocket(sendSocket);
    WSACleanup();
}

int main() {
    cout << "=== Task 5 Performance Benchmark ===" << endl;
    cout << "Iterations: " << ITERATIONS << endl;
    cout << "Message size: " << TEST_MSG.length() << " bytes" << endl << endl;

    // ==========================================
    // TEST A: Thread Shared Memory Overhead
    // ==========================================
    auto start_thread = high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        // 模拟线程间通信：加锁 -> 写入数据 -> 解锁
        lock_guard<mutex> lock(mtx);
        shared_data = TEST_MSG; 
        // (这里省略了 notify，仅测试数据写入共享内存的开销)
    }

    auto end_thread = high_resolution_clock::now();
    auto duration_thread = duration_cast<microseconds>(end_thread - start_thread).count();

    cout << "[Threads] Total time: " << duration_thread << " microseconds" << endl;
    cout << "[Threads] Average time per message: " << (double)duration_thread / ITERATIONS << " microseconds" << endl << endl;


    // ==========================================
    // TEST B: UDP IPC Overhead
    // ==========================================
    setup_udp();
    auto start_udp = high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        // 模拟进程间通信：系统调用 sendto
        sendto(sendSocket, TEST_MSG.c_str(), TEST_MSG.length(), 0, (SOCKADDR*)&destAddr, sizeof(destAddr));
    }

    auto end_udp = high_resolution_clock::now();
    auto duration_udp = duration_cast<microseconds>(end_udp - start_udp).count();
    cleanup_udp();

    cout << "[UDP] Total time: " << duration_udp << " microseconds" << endl;
    cout << "[UDP] Average time per message: " << (double)duration_udp / ITERATIONS << " microseconds" << endl << endl;

    // ==========================================
    // Comparison
    // ==========================================
    double ratio = (double)duration_udp / (double)duration_thread;
    cout << ">>> Conclusion: UDP is approx " << ratio << " times SLOWER than Threads." << endl;

    return 0;
}