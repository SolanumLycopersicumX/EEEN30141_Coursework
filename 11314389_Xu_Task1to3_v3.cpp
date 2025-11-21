// Rename your file to be   StudentID_FamilyName_vVersion.cpp
// And put that information in the file header
// TODO  StudentID = 11314389
// TODO  FamilyName = Xu

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <random> // For std::uniform_real_distribution, std::mt19937, and std::random_device
#include <condition_variable>
#include <array>
#include <atomic>
// #include <barrier> // Available in C++ >11
#include "barrier.hpp" // Our version of barrier if you are using C++11
#include "cs_helper_DoNotModify.hpp"

using namespace std;

const int NUM_TEAMS = 4;     // number of teams in the race
const int NUM_MEMBERS = 4; // number of athletes in the team

// Data for team/athelete initialisation. The Women’s 4x100 meter relay at the Tokyo 2020 Olympics. The teams took between 41 and 42 seconds.
std::array<string, 4> astrTeams = {"Jamaica", "United States", "Great Britain", "Switzerland"};
std::array<std::array<std::string, 4>, 4> aastrCompetitors = {{{"Williams", "Thompson-Herah", "Fraser-Pryce", "Jackson"},
                                                               {"Oliver", "Daniels", "Prandini", "Thomas"},
                                                               {"Philip", "Lansiquot", "Asher-Smith", "Neita"},
                                                               {"Del-Ponte", "Kambundji", "Kora", "Dietsche"}}};

class RandomTwister
{
public:
    RandomTwister(float min, float max) : distribution(min, max) {} // Initialises uniform_real_distribution to initialize to a specific range of numbers
    float generate()                                                // Returns a random float within the specified range
    {
        // Part 1.1 Make the Random number generator thread-safe by adding a simple std::mutex
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for the duration of this function
        return distribution(engine);
    }

private:
    std::mutex mtx;                                         // Mutex to make the generate() function thread-safe
    std::mt19937 engine{std::random_device{}()};            // Mersenne Twister random number generator engine
    std::uniform_real_distribution<float> distribution; 
};

// Part 1.2 Make thrd_print thread safe.
void thrd_print(const std::string &str)
{                                // Thread safe print
    static std::mutex print_mtx; // Mutex to protect printing
    std::lock_guard<std::mutex> lock(print_mtx);
    cout << str;
}

barrier barrier_allthreads_started(1 + (NUM_TEAMS * NUM_MEMBERS)); 
// Part 1.3 Create another barrier array "barrier_go"
barrier barrier_go(1 + (NUM_TEAMS * NUM_MEMBERS));

// Part 2.1  Create a std::atomic variable "winner"
std::atomic<bool> winner{false}; 

void thd_runner_16x100m(Competitor &a, RandomTwister &generator)
{
    // Task 1 function (Not used in Task 2/3 but kept for completeness)
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
    // 强制刷新缓冲区，确保能看到 "ready"
    thrd_print(a.getPerson() + " ready...\n");
    
    // Part 2.2 Barriers
    barrier_allthreads_started.arrive_and_wait();
    barrier_go.arrive_and_wait();
    
    // 接力逻辑判断
    if (pPrevA == NULL) {
        // 第一棒：枪响直接跑
        thrd_print(a.getPerson() + " started, ");
    }
    else
    {     
        // 后续棒次：等待上一棒
        { 
            // Part 2.3 Create unique_lock
            std::unique_lock<std::mutex> lock(pPrevA->mtx);
            // Part 2.4 Wait for baton
            pPrevA->baton.wait(lock, [&] { return pPrevA->bFinished; });
        }

        // --- Task 3: 掉棒与失误逻辑 (Dropped Baton) --- 
        RandomTwister rand_fumble(0.0f, 1.0f); // 生成 0% 到 100% 的概率
        float fChance = rand_fumble.generate();

        // 3.2 掉棒 (概率 < 5%) [cite: 141]
        if (fChance < 0.05f) 
        {
            thrd_print("\n!!! OOPS! " + pPrevA->getPerson() + " DROPPED the baton passing to " + a.getPerson() + "! Team " + a.getTeamName() + " ELIMINATED! !!!\n");
            // 设置一个巨大的时间，标记该队淘汰，同时唤醒可能的下一棒防止死锁
            a.setTime(9999.0f); 
            return; // 线程直接结束，不跑了
        }
        // 3.1 失误 (5% <= 概率 < 20%) [cite: 140]
        else if (fChance < 0.20f)
        {
            // 延迟时间 = 概率 * 10 (例如 15% -> 1.5秒)
            float fDelay_s = fChance * 10.0f; 
            thrd_print("... " + a.getPerson() + " FUMBLED the baton! Delay: " + std::to_string(fDelay_s) + "s\n");
            // 模拟捡棒子的耗时
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(fDelay_s * 1000.0f)));
        }
        
        thrd_print(a.getPerson() + " (" + a.getTeamName() + ")" + " took the baton from " + pPrevA->getPerson() + " (" + pPrevA->getTeamName() + ")\n");
    }

    // Part 2.5 模拟跑步
    float fSprintDuration_seconds = generator.generate(); 
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fSprintDuration_seconds * 1000.0f)));
    
    a.setTime(fSprintDuration_seconds);
    
    thrd_print("Leg " + std::to_string(a.numBatonExchanges()) + ": " + a.getPerson() + " ran in " + std::to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");
    
    // Part 2.6 冠军判定
    if (a.numBatonExchanges() == NUM_MEMBERS) 
    {
        if (!winner.exchange(true)) 
        {
            std::cout << "\n Team " << a.getTeamName() << " is the WINNER!" << std::endl;
        }
    }
}

int main()
{
    thread thread_competitor[NUM_TEAMS][NUM_MEMBERS]; 
    Team aTeams[NUM_TEAMS];                           
    Competitor athlete[NUM_TEAMS][NUM_MEMBERS];       
    float afTeamTime_s[NUM_TEAMS];

    // Part 1.7: 跑步时间 10-12s
    RandomTwister randGen_sprint_time(10.0f, 12.0f);
    
    std::cout << "Re-run of the women’s 4x100 meter relay at the Tokyo 2020 Olympics.\n" << std::endl;

    // 创建线程循环
    for (int i = 0; i < NUM_TEAMS; ++i)
    {
        afTeamTime_s[i] = 0;
        aTeams[i].setTeam(astrTeams[i]);
        for (int j = 0; j < NUM_MEMBERS; ++j)
        {
            athlete[i][j].set(aastrCompetitors[i][j], &(aTeams[i])); 
            
            // Part 2.7: 启动接力赛线程
            if ( j==0 )  
            {
                // 第一棒
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor*)nullptr, std::ref(randGen_sprint_time));
            }
            else
            {
                // 后续棒次
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor *)&(athlete[i][j - 1]), std::ref(randGen_sprint_time)); 
            }
        }
    }

    // Part 1.9: 裁判等待所有运动员就位
    barrier_allthreads_started.arrive_and_wait();
    
    thrd_print("\n\nThe race official raises her starting pistol...\n");
    
    // Part 2.8: 随机发令枪时间 3-5s
    RandomTwister randGen_starter(3.0f, 5.0f);
    float fStarterGun_s = randGen_starter.generate();
    
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fStarterGun_s * 1000.0f)));
    
    // Part 1.11: 裁判发令
    barrier_go.arrive_and_wait(); 
    
    thrd_print("\nGO !\n\n");
    
    // Part 1.12: 等待所有线程结束
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

    std::cout << "\n\nTEAM RESULTS" << std::endl;
    for (int i = 0; i < NUM_TEAMS; ++i)
        aTeams[i].printTimes();
    std::cout << std::endl;
    return 0;
}