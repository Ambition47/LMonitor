#include "thread/ThreadPool.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>


// ============================================================
// Constructor
// ============================================================

ThreadPool::ThreadPool(
    std::size_t threadCount,
    std::size_t maxQueueSize
)
    : maxQueueSize_(
          maxQueueSize
      ) {

    if (threadCount == 0) {

        throw std::invalid_argument(
            "ThreadPool thread count must be greater than 0"
        );
    }


    if (maxQueueSize_ == 0) {

        throw std::invalid_argument(
            "ThreadPool max queue size must be greater than 0"
        );
    }


    workers_.reserve(
        threadCount
    );


    try {

        for (std::size_t i = 0;
             i < threadCount;
             ++i) {

            workers_.emplace_back(
                [this]() {
                    workerLoop();
                }
            );
        }

    } catch (...) {

        {
            std::lock_guard<std::mutex>
                lock(
                    mutex_
                );

            stopping_ =
                true;
        }


        condition_.notify_all();


        for (auto& worker :
             workers_) {

            if (worker.joinable()) {

                worker.join();
            }
        }


        throw;
    }
}


// ============================================================
// Destructor
// ============================================================

ThreadPool::~ThreadPool() {

    stop();
}


// ============================================================
// Try to submit task
// ============================================================

bool ThreadPool::trySubmit(
    Task task
) {
    if (!task) {
        return false;
    }


    {
        std::lock_guard<std::mutex>
            lock(
                mutex_
            );


        if (stopping_) {

            return false;
        }


        // ----------------------------------------------------
        // Backpressure:
        //
        // Never allow the pending task queue to grow
        // without limit.
        // ----------------------------------------------------

        if (tasks_.size() >=
            maxQueueSize_) {

            return false;
        }


        tasks_.push(
            std::move(
                task
            )
        );
    }


    condition_.notify_one();


    return true;
}


// ============================================================
// Graceful stop
// ============================================================

void ThreadPool::stop() {

    {
        std::lock_guard<std::mutex>
            lock(
                mutex_
            );


        if (stopping_) {

            return;
        }


        stopping_ =
            true;
    }


    condition_.notify_all();


    for (auto& worker :
         workers_) {

        if (worker.joinable()) {

            worker.join();
        }
    }


    workers_.clear();
}


// ============================================================
// Worker count
// ============================================================

std::size_t ThreadPool::threadCount() const {

    return workers_.size();
}


// ============================================================
// Current pending task count
// ============================================================

std::size_t ThreadPool::queueSize() const {

    std::lock_guard<std::mutex>
        lock(
            mutex_
        );


    return tasks_.size();
}


// ============================================================
// Maximum pending task count
// ============================================================

std::size_t ThreadPool::maxQueueSize() const noexcept {

    return maxQueueSize_;
}


// ============================================================
// Worker loop
// ============================================================

void ThreadPool::workerLoop() {

    while (true) {

        Task task;


        {
            std::unique_lock<std::mutex>
                lock(
                    mutex_
                );


            condition_.wait(
                lock,
                [this]() {

                    return stopping_ ||
                           !tasks_.empty();
                }
            );


            // Graceful shutdown:
            //
            // Stop only when shutdown was requested
            // AND all already queued tasks are finished.
            if (stopping_ &&
                tasks_.empty()) {

                return;
            }


            task =
                std::move(
                    tasks_.front()
                );


            tasks_.pop();
        }


        // Execute outside mutex.
        try {

            task();

        } catch (
            const std::exception& e
        ) {

            std::cerr
                << "ThreadPool task exception: "
                << e.what()
                << '\n';

        } catch (...) {

            std::cerr
                << "ThreadPool task threw unknown exception\n";
        }
    }
}
