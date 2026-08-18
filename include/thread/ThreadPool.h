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
        std::size_t threadCount
    );


    ~ThreadPool();


    ThreadPool(
        const ThreadPool&
    ) = delete;


    ThreadPool& operator=(
        const ThreadPool&
    ) = delete;


    // Submit one task to the worker queue.
    void submit(
        Task task
    );


    // Gracefully stop workers.
    //
    // Already queued tasks will be completed
    // before worker threads exit.
    void stop();


    std::size_t threadCount() const;


private:
    void workerLoop();


private:
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
