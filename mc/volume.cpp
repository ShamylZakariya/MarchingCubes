//
//  volume.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 01/05/19.
//  Copyright © 2020 Shamyl Zakariya. All rights reserved.
//

#include <atomic>

#include "volume.hpp"

using namespace glm;

namespace mc {

void OctreeVolume::march(
    std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
    if (!_marchThreads) {
        auto nThreads = _triangleConsumers.size();
        _marchThreads = std::make_unique<util::ThreadPool>(nThreads, true);
    }

    _nodesToMarch.clear();
    collect(_nodesToMarch);

    // if we hav an observer, pass collected march nodes to it
    if (marchedNodeObserver) {
        for (const auto& node : _nodesToMarch) {
            marchedNodeObserver(node);
        }
    }

    // collapse each node's set of samplers to a vector for faster iteration when marching
    for (const auto& node : _nodesToMarch) {
        node->_additiveSamplersVec.clear();
        node->_subtractiveSamplersVec.clear();

        std::copy(std::begin(node->additiveSamplers),
            std::end(node->additiveSamplers),
            std::back_inserter(node->_additiveSamplersVec));

        std::copy(std::begin(node->subtractiveSamplers),
            std::end(node->subtractiveSamplers),
            std::back_inserter(node->_subtractiveSamplersVec));
    }

    for (auto& tc : _triangleConsumers) {
        tc->start();
    }

    std::mutex popMutex;
    for (std::size_t i = 0, N = _marchThreads->size(); i < N; i++) {
        _marchThreads->enqueue([&popMutex, this, N](int threadIdx) {
            while (true) {
                Node* node = nullptr;
                {
                    std::lock_guard<std::mutex> popLock(popMutex);
                    if (_nodesToMarch.empty()) {
                        // we're done, exit the loop
                        return;
                    }
                    node = _nodesToMarch.back();
                    _nodesToMarch.pop_back();
                }

                marchNode(node, *_triangleConsumers[threadIdx % N]);
            }
        });
    }

    _marchThreads->wait();

    for (auto& tc : _triangleConsumers) {
        tc->finish();
    }
}

void OctreeVolume::marchNode(OctreeVolume::Node* node, TriangleConsumer<Vertex>& tc)
{
    auto fuzziness = this->_fuzziness;
    auto valueSampler = [fuzziness, node](const vec3& p, mc::MaterialState &material) {
        float value = 0;
        for (auto additiveSampler : node->_additiveSamplersVec) {
            value += additiveSampler->valueAt(p, fuzziness);
        }
        value = min<float>(value, 1.0F);
        for (auto subtractiveSampler : node->_subtractiveSamplersVec) {
            value -= subtractiveSampler->valueAt(p, fuzziness);
        }
        value = max<float>(value, 0.0F);
        return value;
    };
    mc::march(util::iAABB(node->bounds), valueSampler, tc);
}

} // namespace mc