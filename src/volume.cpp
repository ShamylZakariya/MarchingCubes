//
//  volume.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 01/05/19.
//  Copyright © 2020 Shamyl Zakariya. All rights reserved.
//

#include "volume.hpp"

#include <atomic>

void OctreeVolume::march(
    const mat4& transform,
    bool computeNormals)
{
    if (!_marchThreads) {
        auto nThreads = _triangleConsumers.size();
        _marchThreads = std::make_unique<ThreadPool>(nThreads, true);
    }

    _marchNodes.clear();
    collect(_marchNodes);

    for (auto &tc : _triangleConsumers) {
        tc->start();
    }

    std::mutex popMutex;
    for (std::size_t i = 0, N = _marchThreads->size(); i < N; i++) {
        _marchThreads->enqueue([&popMutex, this, i, N, &transform, computeNormals]() {
            while (true) {
                Node* node = nullptr;
                {
                    std::lock_guard<std::mutex> popLock(popMutex);
                    if (_marchNodes.empty()) {
                        return;
                    }
                    node = _marchNodes.back();
                    _marchNodes.pop_back();
                }

                // FIXME: we shouldn't be calling march() - it doesn't know which samplers to use
                // we need to march the sub-region against the set of samplers in node

                mc::march(*this, node->bounds, *_triangleConsumers[i % N], transform, computeNormals);
            }
        });
    }

    _marchThreads->wait();

    for (auto &tc : _triangleConsumers) {
        tc->finish();
    }
}
