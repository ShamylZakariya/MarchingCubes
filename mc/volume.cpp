//
//  volume.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 01/05/19.
//  Copyright © 2020 Shamyl Zakariya. All rights reserved.
//

#include <atomic>
#include <iostream>

#include "util/op_queue.hpp"
#include "volume.hpp"

using namespace glm;

namespace mc {

void OctreeVolume::march(
    std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
    marchSetup(marchedNodeObserver);
    auto jobs = marchCollectedNodes();

    // blocking wait
    for (auto& j : jobs) {
        j.wait();
    }

    for (auto& tc : _triangleConsumers) {
        tc->finish();
    }
}

void OctreeVolume::marchAsync(
    std::function<void()> onReady,
    std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
    _asyncMarchId++;
    auto id = _asyncMarchId;

    marchSetup(marchedNodeObserver);
    auto jobs = marchCollectedNodes();

    // make a job to wait for march to complete, and subsequently notify main thread
    _asyncMarchWaiter = _threadPool->enqueue([this, onReady, id, jobs { std::move(jobs) }](int threadIdx) {
        if (id != _asyncMarchId) {
            // looks like a new march got queued before this one
            // finished, so bail on this pass
            std::cout << "[OctreeVolume::marchAsync] - waitPool - expected id: "
                      << id << " but current asyncMarchId is: "
                      << _asyncMarchId << " bailing." << std::endl;

            return;
        }

        for (auto& j : jobs) {
            j.wait();
        }

        util::MainThreadQueue()->add([this, onReady]() {
            for (auto& tc : _triangleConsumers) {
                tc->finish();
            }
            onReady();
        });
    });
}

void OctreeVolume::marchSetup(std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
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
}

std::vector<std::future<void>> OctreeVolume::marchCollectedNodes()
{
    std::vector<std::future<void>> jobs;
    for (std::size_t i = 0, N = _threadPool->size(); i < N; i++) {
        jobs.push_back(_threadPool->enqueue([this, N](int threadIdx) {
            while (true) {
                Node* node = nullptr;
                {
                    std::lock_guard<std::mutex> popLock(_queuePopMutex);
                    if (_nodesToMarch.empty()) {
                        // we're done, exit the loop
                        return;
                    }
                    node = _nodesToMarch.back();
                    _nodesToMarch.pop_back();
                }

                marchNode(node, *_triangleConsumers[threadIdx % N]);
            }
        }));
    }
    return jobs;
}

void OctreeVolume::marchNode(OctreeVolume::Node* node, TriangleConsumer<Vertex>& tc)
{
    auto fuzziness = this->_fuzziness;
    auto valueSampler = [fuzziness, node](const vec3& p, mc::MaterialState& material) {
        // run additive samplers, interpolating
        // material state
        float value = 0;
        for (auto additiveSampler : node->_additiveSamplersVec) {
            MaterialState m;
            auto v = additiveSampler->valueAt(p, fuzziness, m);
            if (value == 0) {
                material = m;
            } else {
                material = mix(material, m, v);
            }
            value += v;
        }

        // run subtractions (these don't affect material state)
        value = min<float>(value, 1.0F);
        for (auto subtractiveSampler : node->_subtractiveSamplersVec) {
            MaterialState _;
            value -= subtractiveSampler->valueAt(p, fuzziness, _);
        }
        value = max<float>(value, 0.0F);

        return value;
    };
    mc::march(util::iAABB(node->bounds), valueSampler, tc);
}

} // namespace mc