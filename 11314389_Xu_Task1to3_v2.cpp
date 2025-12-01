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
        // Part 1.1 Make the Random number generator thread-safe by adding a simple std::mutex, and rembember the unlock!  Instantiate the mutex into private: area of this class, below
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for the duration of this function
        return distribution(engine);
    }

private:
    // std::random_device creates a seed value for the mt19937 instance creation. It creates a seed value for the “mt” random number generator
    std::mutex mtx;                                         // Mutex to make the generate() function thread-safe
    std::mt19937 engine{std::random_device{}()};            // Mersenne Twister random number generator engine, with a seed from random_device() - static
    std::uniform_real_distribution<float> distribution; // This uniform_real_distribution transforms the engine output into the required (min, max) range and data type.
};

// Part 1.2 Make thrd_print thread safe.  Instantiate a mutex here (global as it is shared between threads) and use it to protect the function using a std::lock_guard<std::mutex>
void thrd_print(const std::string &str)
{                                // Thread safe print
    static std::mutex print_mtx; // Mutex to protect printing
    std::lock_guard<std::mutex> lock(print_mtx);
    cout << str;
}

barrier barrier_allthreads_started(1 + (NUM_TEAMS * NUM_MEMBERS)); // Need all the thread to reach here before the start can continue.
// Part 1.3 Create another barrier array and name it "barrier_go" which you will use to make all threads wait until the race official starts the race
barrier barrier_go(1 + (NUM_TEAMS * NUM_MEMBERS));
// Part 2.1  Create a std::atomic variable of type bool, initalised to false and name it "winner". You will use it to ensure just the winning thread claims to have won the race.
std::atomic<bool> winner{false}; // 初始化为 false

void thd_runner_16x100m(Competitor &a, RandomTwister &generator)
{
    thrd_print(a.getPerson() + " ready, ");
    // Wait for all *17* threads to ensure that all threads have started (all athletes are on their starting blocks!)
    // Part 1.4 Apply the barrier_allthreads_started using arrive_and_wait().
    // Wait at the barrier until all threads are running (including the main thread this is 16+1)
    // Wait for the starter gun to fire (all threads will be waiting while the random countdown for the starting pistol). Then the main thread also waits making 17 threads and this allows them all to started running
    barrier_allthreads_started.arrive_and_wait();
    // Part 1.5 Apply the barrier_go here
    barrier_go.arrive_and_wait();
    thrd_print(a.getPerson() + " started, ");             // This is an individual race, all the competitors are starting at the same time.
    float fSprintDuration_seconds = generator.generate(); // Thread sleep for the random time period (between 10 s and 12 s.
    // Part 1.6 Add a this_thread sleep_for to sleep for the SprintDuration. Convert the float seconds to milliseconds, and then to integer and then use std::chrono::milliseconds() to convert it to a time unit for sleep_for
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fSprintDuration_seconds * 1000.0f)));
    a.setTime(fSprintDuration_seconds); // Update the competitor and team information
    thrd_print(a.getPerson() + " took " + std::to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");
}

void thd_runner_4x4x100m(Competitor &a, Competitor *pPrevA, RandomTwister &generator)
{
    thrd_print(a.getPerson() + " ready...\n");
    // Part 2.2 Copy the code from thd_runner_16x100m for
    // Part 2.2: 像 Task 1 一样等待起跑 (复制 Task 1 的逻辑)
    barrier_allthreads_started.arrive_and_wait();
    barrier_go.arrive_and_wait();
    
    // thrd_print(a.getPerson() + " started, "); // [Professor Note: Fixed logic, removed duplicate print]
    
    // If the competitor does not have a pointer to a previous competitor, then it must be the first runner of that team.
    // 接力逻辑判断
    if (pPrevA == NULL)
        // 如果是第一棒 (没有前一棒)，枪响直接跑
        thrd_print(a.getPerson() + " started, ");
    else
    {     // If they are not the first runner in that team, then they need to wait for the previous runner to give them the baton.
          // 如果不是第一棒，必须等待上一棒 (pPrevA) 递交接力棒
        { // Brackets to reduce mutex scope
          // Part 2.3 Create a std::unique_lock<std::mutex> called "lock", initialised with pPrevA->mtx mutex
          // Part 2.3: 创建锁，锁住上一棒的互斥量
          std::unique_lock<std::mutex> lock(pPrevA->mtx);//unique_lock：condition_variable::wait()函数要求必须传入 unique_lock
          // Part 2.4 Complete the pPrevA->baton condition_variable line below to wait on that lock. (use the pPrevA->bFinished as the check function. It is tricky to get the lambda right!)
          // pPrevA->baton.wait(lock, ... Complete this bit ... }); // Wait for the baton to arrive.
          // Part 2.4: 等待接力棒 (Condition Variable)
          // 解释: wait 会让线程休眠。当 pPrevA 完成并 notify 时，或者 bFinished 变 true 时，线程唤醒。
          pPrevA->baton.wait(lock, [&] { return pPrevA->bFinished; });
          //线程在这里暂停，释放锁，进入阻塞状态，等待上一棒发出信号。即使没有收到信号，操作系统有时也会莫名其妙唤醒线程。如果没有这个 bFinished 检查，线程可能还没接到棒就跑了。
          //wait 被唤醒时，会重新加锁，然后检查 bFinished。如果 bFinished 是 true（上一棒跑完了），就继续执行；如果是 false（可能被误唤醒），就释放锁继续睡。
        }
        thrd_print(a.getPerson() + " (" + a.getTeamName() + ")" + " took the baton from " + pPrevA->getPerson() + " (" + pPrevA->getTeamName() + ")\n");
    }
    // Part 2.5 Copy the code from thd_runner_16x100m for fSprintDuration_seconds and std::this_thread::sleep_for
    // Part 2.5: 模拟跑步 (复制 Task 1 的逻辑)
    float fSprintDuration_seconds = generator.generate(); 
                                                            
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fSprintDuration_seconds * 1000.0f)));
    a.setTime(fSprintDuration_seconds);
    thrd_print("Leg " + std::to_string(a.numBatonExchanges()) + ": " + a.getPerson() + " ran in " + std::to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");
    // 检查是否是最后一棒 (第4棒)
    if (a.numBatonExchanges() == NUM_MEMBERS) // The last athlete in the team has crossed the finish line (crossing the line counts as a baton exchage)
    {
        // Print "finished" only if this is the first thread to complete
        // Part 2.6 Use an atomic .exchange on the atomic "winner" object that you defined at the top of this code and use this in the line below
        // Part 2.6: 只有第一个将 winner 从 false 改为 true 的线程才是赢家
        if (!winner.exchange(true)) // exchange是原子操作，返回旧值并设置新值，返回false表示之前没有赢家，并将true写入winner，让第二个到达的线程返回true，第二个线程就会跳过打印
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

    RandomTwister randGen_sprint_time(10.0f, 12.0f);
    
    std::cout << "Re-run of the women’s 4x100 meter relay at the Tokyo 2020 Olympics.\n" << std::endl;

    // --- 循环开始 ---
    for (int i = 0; i < NUM_TEAMS; ++i)
    {
        afTeamTime_s[i] = 0;
        aTeams[i].setTeam(astrTeams[i]);
        for (int j = 0; j < NUM_MEMBERS; ++j)
        {
            athlete[i][j].set(aastrCompetitors[i][j], &(aTeams[i])); 
            
            // 创建线程
            if ( j==0 )  
            {
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor*)nullptr, std::ref(randGen_sprint_time));
            }
            else
            {
                thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor *)&(athlete[i][j - 1]), std::ref(randGen_sprint_time)); 
            }
        } // 结束内层循环 (j)
    } // 结束外层循环 (i) <--- 【关键】必须在这里结束循环！
    // --- 循环结束 ---

    // 【关键】屏障必须在所有循环都结束后才能调用
    // Wait at the barrier until all threads arrive (16 athletes + 1 main thread)
    barrier_allthreads_started.arrive_and_wait();
    
    thrd_print("\n\nThe race official raises her starting pistol...\n");
    
    RandomTwister randGen_starter(3.0f, 5.0f);
    float fStarterGun_s = randGen_starter.generate();
    
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(fStarterGun_s * 1000.0f)));
    
    barrier_go.arrive_and_wait(); // 裁判发令
    
    thrd_print("\nGO !\n\n");
    
    // Join all threads
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