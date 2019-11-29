//
//  marching_cubes.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef marching_cubes_hpp
#define marching_cubes_hpp

#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "aabb.hpp"
#include "triangle_soup.hpp"
#include "unowned_ptr.hpp"
#include "util.hpp"

namespace mc {

#pragma mark - IIsoSurface

class IIsoSurface {
public:
    IIsoSurface() = default;
    virtual ~IIsoSurface() = default;

    /*
     Return the bounded size of the isosurface volume
     */
    virtual ivec3 size() const = 0;

    /*
     Return the value of the isosurface at a point in space, where 0 means "nothing there" and 1 means fully occupied.
     */
    virtual float valueAt(const vec3& p) const = 0;

    /*
     Return the normal of the gradient at a point in space.
     Default implementation takes 6 taps about the sample point to compute local gradient.
     Custom IsoSurface implementations may be able to implement a smarter domain-specific approach.
     */
    virtual vec3 normalAt(const vec3& p) const;
};

#pragma mark - Marching

void march(const IIsoSurface& surface,
    ITriangleConsumer& tc,
    float isolevel = 0.5f,
    const mat4& transform = mat4(1),
    bool computeNormals = true);

void march(const mc::IIsoSurface& volume, AABBi region, ITriangleConsumer& tc, float isolevel = 0.5, const mat4& transform = mat4(1), bool computeNormals = true);

void march(const IIsoSurface& surface,
    const std::vector<unowned_ptr<ITriangleConsumer>>& tc,
    float isoLevel = 0.5f,
    const mat4& transform = mat4(1),
    bool computeNormals = true);

namespace {
    class LooperThread {
    public:
        LooperThread(std::function<void()> &&work, int affinity = -1)
            : _work(std::move(work))
            , _affinity(affinity)
        {
            launchThread();
        }

        ~LooperThread()
        {
            std::lock_guard<std::mutex> threadLock(_threadMutex);
            terminateThread();
        }

        void runOnce()
        {
            std::cout << "runOnce()" << std::endl;
            std::lock_guard<std::mutex> lock(_workMutex);
            _runRequested = true;
            std::cout << "runOnce() - set runRequested, notifying..." << std::endl;
            _workCondition.notify_all();
        }

        void wait()
        {
            std::unique_lock<std::mutex> lock(_waitMutex);
            _waitCondition.wait(lock);
        }

    private:
        void launchThread()
        {
            std::lock_guard<std::mutex> threadLock(_threadMutex);
            if (_thread.joinable()) {
                terminateThread();
            }
            _thread = std::thread([this]() {
                if (_affinity >= 0) {
                    // not available on macos!?
                }
                threadLoop();
            });
        }

        void terminateThread()
        {
            {
                std::lock_guard<std::mutex> workLock(_workMutex);
                _isActive = false;
                _workCondition.notify_all();
            }
            _thread.join();
        }

        void threadLoop()
        {
            std::lock_guard<std::mutex> lock(_workMutex);
            while (_isActive) {
                std::cout << "Waiting on _workCondition" << std::endl;
                _workCondition.wait(_workMutex, [this](){ return _runRequested || !_isActive; });

                _work();
                _runRequested = false;
                _waitCondition.notify_all();
            }
        }

        int _affinity = -1;
        std::mutex _threadMutex;
        std::thread _thread;

        std::mutex _workMutex;
        std::function<void()> _work;
        bool _isActive = true;
        bool _runRequested = false;
        std::condition_variable_any _workCondition;

        std::mutex _waitMutex;
        std::condition_variable_any _waitCondition;
    };
}

class ThreadedMarcher {
public:
    ThreadedMarcher(const IIsoSurface& surface,
        const std::vector<unowned_ptr<ITriangleConsumer>>& tc,
        float isoLevel = 0.5f,
        const mat4& transform = mat4(1),
        bool computeNormals = true)
        : _surface(surface)
        , _consumers(tc)
        , _isoLevel(isoLevel)
        , _transform(transform)
        , _computeNormals(computeNormals)
    {
    }

    ~ThreadedMarcher() = default;

    void march();

private:
    const IIsoSurface& _surface;
    std::vector<unowned_ptr<ITriangleConsumer>> _consumers;
    std::vector<std::unique_ptr<LooperThread>> _threads;
    float _isoLevel;
    mat4 _transform;
    bool _computeNormals;
};

}

#endif /* marching_cubes_hpp */
