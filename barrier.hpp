#ifndef cs_barrier_file_hpp
#define cs_barrier_file_hpp

#include <thread>
#include <mutex>

class barrier {
public:
    explicit barrier(std::size_t num_threads)
        : num_threads(num_threads), count(num_threads), generation(0) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mtx);
        auto current_generation = generation;
        if (--count == 0) {
            // All threads have reached the barrier
            generation++;  // Move to the next generation
            count = num_threads;  // Reset the count for the next barrier
            cv.notify_all();  // Wake up all threads
        } else {
            // Wait until the last thread reaches the barrier
            cv.wait(lock, [this, current_generation] { return current_generation != generation; });
        }
    }
private:
    std::mutex mtx;  // Mutex is non-movable prior to C++17 so you cannot create a vector array of them
    std::condition_variable cv;
    std::size_t num_threads, count, generation;  // generation is to track generations of barrier resets
};

#endif // cs_barrier_file_hpp 
