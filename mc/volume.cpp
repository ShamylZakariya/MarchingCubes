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

using namespace glm;

namespace mc {

void OctreeVolume::dispatchMarch(
    util::unowned_ptr<glm::mat4> transform,
    bool computeNormals,
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
        _marchThreads->enqueue([&popMutex, this, i, N, &transform, computeNormals]() {
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

                marchNode(node, *_triangleConsumers[i % N], transform, computeNormals);
            }
        });
    }

    _marchThreads->wait();

    for (auto& tc : _triangleConsumers) {
        tc->finish();
    }
}

void OctreeVolume::marchNode(OctreeVolume::Node* node, TriangleConsumer<util::VertexP3C4N3>& tc, util::unowned_ptr<glm::mat4> transform, bool computeNormals)
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

    auto normalSampler = [fuzziness, node](const vec3& p) {
        vec3 normal { 0 };

        for (auto additiveSampler : node->_additiveSamplersVec) {
            normal += additiveSampler->normalAt(p, fuzziness);
        }

        normal = normalize(normal);

        for (auto subtractiveSampler : node->_subtractiveSamplersVec) {
            normal -= subtractiveSampler->normalAt(p, fuzziness);
        }

        normal = normalize(normal);
        return normal;
    };

    mc::march(util::iAABB(node->bounds), valueSampler, normalSampler, tc, transform);
}

} // namespace mc