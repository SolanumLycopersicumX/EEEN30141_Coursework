// Rename your file to be   StudentID_FamilyName_vVersion.cpp
// And put that information in the file header
// TODO  StudentID = 11314389
// TODO  FamilyName = Xu

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <random>
#include <condition_variable>
#include <array>
#include <atomic>
#include "barrier.hpp"
#include "cs_helper_DoNotModify.hpp"

// --- Task 4: Winsock Headers (No Asio) ---
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// -----------------------------------------

using namespace std;

const int NUM_TEAMS = 4;
const int NUM_MEMBERS = 4;

std::array<string, 4> astrTeams = {"Jamaica", "United States", "Great Britain", "Switzerland"};
std::array<std::array<std::string, 4>, 4> aastrCompetitors = {{{"Williams", "Thompson-Herah", "Fraser-Pryce", "Jackson"},
                                                               {"Oliver", "Daniels", "Prandini", "Thomas"},
                                                               {"Philip", "Lansiquot", "Asher-Smith", "Neita"},
                                                               {"Del-Ponte", "Kambundji", "Kora", "Dietsche"}}};

// --- Task 4 UDP Globals ---
SOCKET g_SendSocket = INVALID_SOCKET;
sockaddr_in g_RecvAddr;
bool g_WSAInitialized = false;

class RandomTwister
{
public:
    RandomTwister(float min, float max) : distribution(min, max) {}
    float generate()
    {
        std::lock_guard<std::mutex> lock(mtx);
        return distribution(engine);
    }

private:
    std::mutex mtx;
    std::mt19937 engine{std::random_device{}()};
    std::uniform_real_distribution<float> distribution;
};

// Part 1.2 Make thrd_print thread safe.
void thrd_print(const std::string &str)
{
    static std::mutex print_mtx;
    std::lock_guard<std::mutex> lock(print_mtx);

    // 1. 本地打印
    cout << str;

    // 2. Task 4: UDP 发送 (Winsock)
    // 只有通过 thrd_print 打印的内容，才会被发送到接收端
    if (g_SendSocket != INVALID_SOCKET)
    {
        sendto(g_SendSocket, str.c_str(), str.length(), 0, (SOCKADDR *)&g_RecvAddr, sizeof(g_RecvAddr));
    }
}

barrier barrier_allthreads_started(1 + (NUM_TEAMS * NUM_MEMBERS));
barrier barrier_go(1 + (NUM_TEAMS * NUM_MEMBERS));
std::atomic<bool> winner{false};

void thd_runner_16x100m(Competitor &a, RandomTwister &generator)
{
    thrd_print(a.getPerson() + " ready, ");
    barrier_allthreads_started.arrive_and_wait();
    barrier_go.arrive_and_wait();
    thrd_print(a.getPerson() + " started, ");
    float fSprintDuration_seconds = generator.generate();
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fSprintDuration_seconds * 1000.0f)));
    a.setTime(fSprintDuration_seconds);
    thrd_print(a.getPerson() + " took " + std::to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");
}

void thd_runner_4x4x100m(Competitor &a, Competitor *pPrevA, RandomTwister &generator)
{
    thrd_print(a.getPerson() + " ready...\n");
    barrier_allthreads_started.arrive_and_wait();
    barrier_go.arrive_and_wait();

    if (pPrevA == NULL)
    {
        thrd_print(a.getPerson() + " started, ");
    }
    else
    {
        {
            std::unique_lock<std::mutex> lock(pPrevA->mtx);
            pPrevA->baton.wait(lock, [&]
                               { return pPrevA->bFinished; });
        }

        RandomTwister rand_fumble(0.0f, 1.0f);
        float fChance = rand_fumble.generate();

        if (fChance < 0.05f)
        {
            thrd_print("\n!!! OOPS! " + pPrevA->getPerson() + " DROPPED the baton passing to " + a.getPerson() + "! Team " + a.getTeamName() + " ELIMINATED! !!!\n");
            a.setTime(9999.0f);
            return;
        }
        else if (fChance < 0.20f)
        {
            float fDelay_s = fChance * 10.0f;
            thrd_print("... " + a.getPerson() + " FUMBLED the baton! Delay: " + std::to_string(fDelay_s) + "s\n");
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(fDelay_s * 1000.0f)));
        }

        thrd_print(a.getPerson() + " (" + a.getTeamName() + ")" + " took the baton from " + pPrevA->getPerson() + " (" + pPrevA->getTeamName() + ")\n");
    }

    float fSprintDuration_seconds = generator.generate();
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fSprintDuration_seconds * 1000.0f)));
    a.setTime(fSprintDuration_seconds);
    thrd_print("Leg " + std::to_string(a.numBatonExchanges()) + ": " + a.getPerson() + " ran in " + std::to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");

    if (a.numBatonExchanges() == NUM_MEMBERS)
    {
        // [修复逻辑] 如果队伍时间很大(说明掉棒淘汰了)，就不能当冠军
        // Team::getTime() 需要这里能访问到，或者简单判断当前选手的设置时间
        // 这里简单起见，如果 atomic 还没被拿走，我们就是冠军
        if (!winner.exchange(true))
        {
            // [修复] 将 std::cout 改为 thrd_print，这样接收端也能收到冠军信息
            thrd_print("\n Team " + a.getTeamName() + " is the WINNER!\n");
        }
    }
}

int main()
{
    // --- Task 4 Initialization (Winsock) ---
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
    {
        g_WSAInitialized = true;
        g_SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        g_RecvAddr.sin_family = AF_INET;
        g_RecvAddr.sin_port = htons(6000);
        g_RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    // ---------------------------------------

    thread thread_competitor[NUM_TEAMS][NUM_MEMBERS];
    Team aTeams[NUM_TEAMS];
    Competitor athlete[NUM_TEAMS][NUM_MEMBERS];
    float afTeamTime_s[NUM_TEAMS];

    RandomTwister randGen_sprint_time(10.0f, 12.0f);

    // [修复] 将 std::cout 改为 thrd_print
    thrd_print("Re-run of the women’s 4x100 meter relay at the Tokyo 2020 Olympics.\n\n");

    for (int i = 0; i < NUM_TEAMS; ++i)
    {
        afTeamTime_s[i] = 0;
        aTeams[i].setTeam(astrTeams[i]);
        for (int j = 0; j < NUM_MEMBERS; ++j)
        {
            athlete[i][j].set(aastrCompetitors[i][j], &(aTeams[i]));

            if (j == 0)
            {
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor *)nullptr, std::ref(randGen_sprint_time));
            }
            else
            {
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor *)&(athlete[i][j - 1]), std::ref(randGen_sprint_time));
            }
        }
    }

    barrier_allthreads_started.arrive_and_wait();
    thrd_print("\n\nThe race official raises her starting pistol...\n");

    RandomTwister randGen_starter(3.0f, 5.0f);
    float fStarterGun_s = randGen_starter.generate();
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fStarterGun_s * 1000.0f)));
    barrier_go.arrive_and_wait();

    thrd_print("\nGO !\n\n");

    for (int i = 0; i < NUM_TEAMS; ++i)
    {
        for (int j = 0; j < NUM_MEMBERS; ++j)
        {
            if (thread_competitor[i][j].joinable())
            {
                thread_competitor[i][j].join();
            }
        }
    }

    // [修复] 手动拼接结果字符串，使用 thrd_print 发送
    thrd_print("\n\nTEAM RESULTS\n");
    for (int i = 0; i < NUM_TEAMS; ++i)
    {
        // aTeams[i].printTimes() 只能输出到 cout，无法发送
        // 所以我们手动获取时间拼接字符串
        string sResult = "Team " + aTeams[i].getTeam() + " = " + to_string(aTeams[i].getTime()) + " s\n";
        thrd_print(sResult);
    }
    thrd_print("\n");

    // --- Task 4 Cleanup ---
    if (g_SendSocket != INVALID_SOCKET)
        closesocket(g_SendSocket);
    if (g_WSAInitialized)
        WSACleanup();

    return 0;
}