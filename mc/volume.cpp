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
    for (auto& tc : _triangleConsumers) {
        tc->start();
    }

    marchSetup();

    // if caller wants to know which nodes were marched
    // we need to make a copy since marchCollectedNodes
    // drains _nodesToMarch
    if (marchedNodeObserver) {
        _marchedNodes = _nodesToMarch;
    }

    auto jobs = marchCollectedNodes();

    // blocking wait
    for (auto& j : jobs) {
        j.wait();
    }

    for (auto& tc : _triangleConsumers) {
        tc->finish();
    }

    // if we hav an observer, pass collected march nodes to it
    if (marchedNodeObserver) {
        for (const auto& node : _marchedNodes) {
            marchedNodeObserver(node);
        }
    }
}

void OctreeVolume::marchAsync(
    std::function<void()> onReady,
    std::function<void(OctreeVolume::Node*)> marchedNodeObserver)
{
    for (auto& tc : _triangleConsumers) {
        tc->start();
    }

    _asyncMarchId++;
    auto id = _asyncMarchId;

    _asyncWaiter = _threadPool->enqueue(
        [this, onReady, marchedNodeObserver, id](int _) {
            // collect the nodes to march
            marchSetup();

            // if caller needs the marched nodes, we need
            // to make a copy
            if (marchedNodeObserver) {
                _marchedNodes = _nodesToMarch;
            }

            // march the collected nodes
            auto jobs = marchCollectedNodes();

            // wait on the march job
            for (auto& j : jobs) {
                j.wait();
            }

            if (id != _asyncMarchId) {
                // looks like a new march got queued before this one
                // finished, so bail on this pass
                std::cout << "[OctreeVolume::marchAsync] - waitPool - expected id: "
                          << id << " but current asyncMarchId is: "
                          << _asyncMarchId << " bailing." << std::endl;

                return;
            }

            util::MainThreadQueue()->add([this, onReady, marchedNodeObserver]() {
                for (auto& tc : _triangleConsumers) {
                    tc->finish();
                }
                onReady();

                // if we hav an observer, pass collected march nodes to it
                if (marchedNodeObserver) {
                    for (const auto& node : _marchedNodes) {
                        marchedNodeObserver(node);
                    }
                }
            });
        });
}

void OctreeVolume::marchSetup()
{
    _nodesToMarch.clear();
    collect(_nodesToMarch);

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
    const auto fuzziness = this->_fuzziness;
    const auto valueSampler = [fuzziness, node](const vec3& p, mc::MaterialState& material) {
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