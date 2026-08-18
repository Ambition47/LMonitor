#include "thread/ThreadPool.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>


// ============================================================
// Constructor
// ============================================================

ThreadPool::ThreadPool(
    std::size_t threadCount
) {
    if (threadCount == 0) {

        throw std::invalid_argument(
            "ThreadPool thread count must be greater than 0"
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

        // ----------------------------------------------------
        // If creating one of the worker threads fails,
        // stop already-created workers before rethrowing.
        // ----------------------------------------------------

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
// Submit task
// ============================================================

void ThreadPool::submit(
    Task task
) {
    if (!task) {
        return;
    }


    {
        std::lock_guard<std::mutex>
            lock(
                mutex_
            );


        if (stopping_) {

            throw std::runtime_error(
                "Cannot submit task to stopped ThreadPool"
            );
        }


        tasks_.push(
            std::move(
                task
            )
        );
    }


    // Wake one sleeping worker.
    condition_.notify_one();
}


// ============================================================
// Stop ThreadPool
// ============================================================

void ThreadPool::stop() {

    {
        std::lock_guard<std::mutex>
            lock(
                mutex_
            );


        // stop() is idempotent.
        if (stopping_) {

            return;
        }


        stopping_ =
            true;
    }


    // Wake every worker so that they can observe stopping_.
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
// Worker main loop
// ============================================================

void ThreadPool::workerLoop() {

    while (true) {

        Task task;


        // ====================================================
        // Wait for:
        //
        // 1. A new task
        // 2. ThreadPool shutdown
        // ====================================================

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


            // ------------------------------------------------
            // Graceful shutdown:
            //
            // stopping_ == true
            // but tasks still exist
            //
            // → continue executing queued tasks.
            //
            // Exit only when:
            //
            // stopping_ == true
            // AND
            // tasks_.empty()
            // ------------------------------------------------

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


        // ====================================================
        // Execute task WITHOUT holding mutex.
        // ====================================================

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
