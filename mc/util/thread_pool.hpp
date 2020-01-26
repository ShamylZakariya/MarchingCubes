//
//  thread_pool.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/30/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_threadpool_h
#define mc_threadpool_h

#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

#ifndef __APPLE__
#include <sched.h>
#endif

namespace mc {
namespace util {

    class ThreadPool {
    public:
        ThreadPool(unsigned int n = std::thread::hardware_concurrency(), bool pinThreads = true)
        {
            for (unsigned int i = 0; i < n; ++i)
                _workers.emplace_back([this, i, pinThreads]() {
                    threadLoop(pinThreads ? i % std::thread::hardware_concurrency() : -1);
                });
        }

        ~ThreadPool()
        {
            // set stop-condition
            std::unique_lock<std::mutex> latch(_queueMutex);
            _stopRequested = true;
            _cvTask.notify_all();
            latch.unlock();

            // all threads terminate, then we're done.
            for (auto& t : _workers)
                t.join();
        }

        template <class F>
        void enqueue(F&& f)
        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _tasks.emplace_back(std::forward<F>(f));
            _cvTask.notify_one();
        }

        void wait()
        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _cvFinished.wait(lock, [this]() { return _tasks.empty() && (_busy == 0); });
        }

        auto size() const { return _workers.size(); }

    private:
        void threadLoop(int pinnedCpuIdx)
        {
            if (pinnedCpuIdx >= 0) {
#ifndef __APPLE__
                cpu_set_t mask;
                CPU_ZERO(&mask);
                CPU_SET(pinnedCpuIdx, &mask);
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
                    std::cerr << "sched_setaffinity failed: " << err << std::endl;
                }
#endif
            }

            while (true) {
                std::unique_lock<std::mutex> latch(_queueMutex);
                _cvTask.wait(latch, [this]() { return _stopRequested || !_tasks.empty(); });

                if (!_tasks.empty()) {
                    ++_busy;

                    auto fn = _tasks.front();
                    _tasks.pop_front();

                    // release lock. run async
                    latch.unlock();

                    // run function outside context
                    fn();

                    latch.lock();
                    --_busy;
                    _cvFinished.notify_one();
                } else if (_stopRequested) {
                    break;
                }
            }
        }

    private:
        std::vector<std::thread> _workers;
        std::deque<std::function<void()>> _tasks;
        std::mutex _queueMutex;
        std::condition_variable _cvTask;
        std::condition_variable _cvFinished;
        unsigned int _busy = 0;
        bool _stopRequested = false;
    };

}
} // namespace mc::util

#endif /* mc_threadpool_h */
