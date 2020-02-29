/**
 * Copyright (c) 2012 Jakob Progsch, Václav Zeman
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *  in a product, an acknowledgment in the product documentation would be
 *  appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 *  misrepresented as being the original software.
 *
 *  3. This notice may not be removed or altered from any source
 *  distribution.
*/

/**
 *  Adapted from: https://github.com/progschj/ThreadPool
 *  Added:
 *      - thread pinning
 *      - pass index of executing thread to callee
 *  Dropped:
 *      - arbitrary task arguments/params
 */

#ifndef thread_pool_hpp
#define thread_pool_hpp

#include <sched.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace mc {
namespace util {

    /**
     * ThreadPool
     * A minimalist thread pool allowing addition of work items, returning a handle
     * for each which may be used to wait on the work's completion.
     * Does not do anything fancy like marshalling arguments or return values.
     */
    class ThreadPool {
    public:
        /**
         * Work function definition. Receives the stable index of the thread
         * executing the job. This will be a value from [0, size())
         */
        typedef std::function<void(int)> WorkFn;

    public:
        /**
         * Create a ThreadPool that will use a specified number of threads,
         * and which will optionally pin those threads to specific CPUs.
         */
        ThreadPool(size_t numThreads, bool pinned);
        ~ThreadPool();

        /**
         * Enqueue a job to execute on the pool.
         * see `WorkFn`; job receives index of thread in pool
         * executing its work.
         * Returns a std::future which can be waited on to block
         * until job is complete.
         */
        template <class F>
        std::future<void> enqueue(F&& f);

        /**
         * Return number of threads being used
         */
        size_t size() const { return _workers.size(); }

    private:
        std::vector<std::thread> _workers;
        std::queue<std::function<void(int)>> _tasks;

        // synchronization
        std::mutex _queue_mutex;
        std::condition_variable _condition;
        bool _stop;
    };

    inline ThreadPool::ThreadPool(size_t numThreads, bool pinned)
        : _stop(false)
    {
        for (size_t i = 0; i < numThreads; ++i)
            _workers.emplace_back(
                [this, i, numThreads, pinned] {
                    if (pinned) {
                        cpu_set_t mask;
                        CPU_ZERO(&mask);
                        CPU_SET(i % numThreads, &mask);
                        auto result = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
                        if (result != 0) {
                            std::string err = "<Unknown>";
                            switch (result) {
                            case EINVAL:
                                err = "EINVAL";
                                break;
                            case EFAULT:
                                err = "EFAULT";
                                break;
                            case EPERM:
                                err = "EPERM";
                                break;
                            case ESRCH:
                                err = "ESRCH";
                                break;
                            }
                            throw std::runtime_error("[ThreadPool::ctor] - sched_setaffinity failed: " + err);
                        }
                    }

                    for (;;) {
                        std::function<void(int)> task;

                        {
                            std::unique_lock<std::mutex> lock(this->_queue_mutex);
                            this->_condition.wait(lock,
                                [this] { return this->_stop || !this->_tasks.empty(); });

                            if (this->_stop && this->_tasks.empty())
                                return;

                            task = std::move(this->_tasks.front());
                            this->_tasks.pop();
                        }

                        task(i);
                    }
                });
    }

    template <class F>
    std::future<void> ThreadPool::enqueue(F&& f)
    {
        auto task = std::make_shared<
            std::packaged_task<void(int)>>(std::bind(std::forward<F>(f), std::placeholders::_1));

        std::future<void> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);

            // don't allow enqueueing after stopping the pool
            if (_stop) {
                throw std::runtime_error("[ThreadPool::enqueue] - enqueue on stopped ThreadPool");
            }

            _tasks.emplace([task](int idx) { (*task)(idx); });
        }
        _condition.notify_one();
        return res;
    }

    inline ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            _stop = true;
        }
        _condition.notify_all();
        for (std::thread& worker : _workers)
            worker.join();
    }

}
}

#endif
