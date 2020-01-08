//
//  thread_pool.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/30/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef thread_pool_h
#define thread_pool_h

#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

class ThreadPool {
public:
    ThreadPool(unsigned int n = std::thread::hardware_concurrency(), bool pinThreads = true)
    {
        for (unsigned int i = 0; i < n; ++i)
            _workers.emplace_back([this, i, pinThreads]() {
                threadLoop(pinThreads ? i : -1);
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
            // TODO(shamyl@gmail.com): set core affinity here
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

#endif /* thread_pool_h */
