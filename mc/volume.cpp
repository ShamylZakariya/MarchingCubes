//
//  volume.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 01/05/19.
//  Copyright © 2020 Shamyl Zakariya. All rights reserved.
//

#include <atomic>

#include "marching_cubes.hpp"
#include "volume.hpp"

void OctreeVolume::march(
    const mat4& transform,
    bool computeNormals,
    std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
    if (!_marchThreads) {
        auto nThreads = _triangleConsumers.size();
        _marchThreads = std::make_unique<ThreadPool>(nThreads, true);
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
        _marchThreads->enqueue([&popMutex, this, i, N, &transform, computeNormals]() {
            while (true) {
                Node* node = nullptr;
                {
                    std::lock_guard<std::mutex> popLock(popMutex);
                    if (_nodesToMarch.empty()) {
                        return;
                    }
                    node = _nodesToMarch.back();
                    _nodesToMarch.pop_back();
                }

                march(node, *_triangleConsumers[i % N], transform, computeNormals);
            }
        });
    }

    _marchThreads->wait();

    for (auto& tc : _triangleConsumers) {
        tc->finish();
    }
}

void OctreeVolume::march(OctreeVolume::Node* node, ITriangleConsumer& tc, const mat4& transform, bool computeNormals)
{
    auto fuzziness = this->_fuzziness;
    auto valueSampler = [fuzziness, node](const vec3& p) {
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

    auto normalSampler = [&valueSampler](const vec3& p) {
        // from GPUGems 3 Chap 1 -- compute normal of voxel space
        const float d = 0.1f;
        vec3 grad(
            valueSampler(p + vec3(d, 0, 0)) - valueSampler(p + vec3(-d, 0, 0)),
            valueSampler(p + vec3(0, d, 0)) - valueSampler(p + vec3(0, -d, 0)),
            valueSampler(p + vec3(0, 0, d)) - valueSampler(p + vec3(0, 0, -d)));

        return -normalize(grad);
    };

    mc::march(iAABB(node->bounds), valueSampler, normalSampler, tc, transform, computeNormals);
}
