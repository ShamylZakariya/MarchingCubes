#include "op_queue.hpp"

namespace mc {
namespace util {

    unowned_ptr<OperationQueue> MainThreadQueue()
    {
        static std::mutex _lock;
        static std::unique_ptr<OperationQueue> _queue = nullptr;

        std::lock_guard lock(_lock);
        if (!_queue) {
            _queue = std::make_unique<OperationQueue>();
        }

        return _queue.get();
    }

}
}