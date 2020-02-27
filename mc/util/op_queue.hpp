#ifndef op_queue_hpp
#define op_queue_hpp

#include <functional>
#include <list>
#include <mutex>

#include "unowned_ptr.hpp"

namespace mc {
namespace util {

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
     * being processed by the app's main thread
     */
    unowned_ptr<OperationQueue> MainThreadQueue();

}
}

#endif