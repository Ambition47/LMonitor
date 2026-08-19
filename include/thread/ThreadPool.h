#ifndef LMONITOR_THREAD_POOL_H
#define LMONITOR_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


class ThreadPool {
public:
    using Task =
        std::function<void()>;


    explicit ThreadPool(
        std::size_t threadCount,
        std::size_t maxQueueSize = 1024
    );


    ~ThreadPool();


    ThreadPool(
        const ThreadPool&
    ) = delete;


    ThreadPool& operator=(
        const ThreadPool&
    ) = delete;


    // Try to submit one task.
    //
    // true:
    //     task accepted
    //
    // false:
    //     queue is full or pool is stopping
    bool trySubmit(
        Task task
    );


    // Gracefully stop workers.
    void stop();


    std::size_t threadCount() const;


    std::size_t queueSize() const;


    std::size_t maxQueueSize() const noexcept;


private:
    void workerLoop();


private:
    std::size_t maxQueueSize_ =
        0;


    std::vector<std::thread>
        workers_;


    std::queue<Task>
        tasks_;


    mutable std::mutex
        mutex_;


    std::condition_variable
        condition_;


    bool stopping_ =
        false;
};

#endif
