#ifndef op_queue_hpp
#define op_queue_hpp

#include <functional>
#include <list>
#include <mutex>

#include "unowned_ptr.hpp"

namespace mc {
namespace util {

    /**
     * OperationQueue is a minimalist task queue.
     * Add work items via add(), execute them via drain().
     * It is expected that the main "app" will drain the
     * MainThreadQueue periodically (i.e. at each iteration of the
     * main loop).
     */
    class OperationQueue {
    public:
        typedef std::function<void()> OperationFn;

        OperationQueue() = default;
        OperationQueue(const OperationQueue&) = delete;
        OperationQueue(OperationQueue&&) = delete;
        ~OperationQueue() = default;

        void add(OperationFn op)
        {
            std::lock_guard lock(_lock);
            _operations.push_back(op);
        }

        void drain()
        {
            std::lock_guard lock(_lock);
            for (const auto& op : _operations) {
                op();
            }
            _operations.clear();
        }

    private:
        std::mutex _lock;
        std::list<OperationFn> _operations;
    };

    /**
     * Get the singleton OperationQueue meant for
     * being processed by the app's main thread.
     */
    unowned_ptr<OperationQueue> MainThreadQueue();

}
}

#endif