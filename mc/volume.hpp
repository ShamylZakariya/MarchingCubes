//
//  volume.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/27/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef volume_hpp
#define volume_hpp

#include <array>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "marching_cubes.hpp"
#include "triangle_consumer.hpp"
#include "util/util.hpp"

namespace mc {

//
// VolumeSampler
//

class IVolumeSampler {
public:
    enum class AABBIntersection {
        None,
        IntersectsAABB,
        ContainsAABB
    };

    enum class Mode {
        Additive,
        Subtractive
    };

public:
    IVolumeSampler(Mode mode)
        : _mode(mode)
    {
    }

    virtual ~IVolumeSampler() = default;

    Mode mode() const { return _mode; }

    /*
     Return true iff bounds intersects the region affected by this sampler
    */
    virtual bool intersects(util::AABB bounds) const = 0;

    /*
    Subtractive samplers can offer an optimization by overriding this method
    to return one of AABBIntersection. In the case that an AABB is completely
    contained by the volume, this method should return AABBIntersection::ContainsAABB
    which allows the OctreeVolume to optimize the set of nodes to march.
    */
    virtual AABBIntersection intersection(util::AABB bounds) const
    {
        return intersects(bounds) ? AABBIntersection::IntersectsAABB : AABBIntersection::None;
    }

    /*
     Return the amount a given point in space is "inside" the volume. The
     fuzziness is a gradient to allow for smoothing. For
     example, in the case of a circle or radius r, a point distance <= r - fuziness
     returns a value of 1, and a distance >= r returns 0,
     and values in between return a linear transition.
     Write the material properties of the volume into `material
     */
    virtual float valueAt(const glm::vec3& p, float fuzziness, MaterialState& material) const = 0;

private:
    Mode _mode;
};

//
// Volume
//

class BaseCompositeVolume {
public:
    BaseCompositeVolume(glm::ivec3 size, float fuzziness)
        : _size(size)
        , _fuzziness(fuzziness)
    {
    }

    virtual ~BaseCompositeVolume() = default;

    template <typename T>
    util::unowned_ptr<T> add(std::unique_ptr<T>&& sampler)
    {
        auto sPtr = sampler.get();
        _samplers.push_back(std::move(sampler));

        switch (sPtr->mode()) {
        case IVolumeSampler::Mode::Additive:
            _additiveSamplers.push_back(sPtr);
            break;

        case IVolumeSampler::Mode::Subtractive:
            _subtractiveSamplers.push_back(sPtr);
            break;
        }

        return sPtr;
    }

    void clear()
    {
        _samplers.clear();
        _additiveSamplers.clear();
        _subtractiveSamplers.clear();
    }

    glm::ivec3 size() const
    {
        return _size;
    }

    void setFuzziness(float ft) { _fuzziness = std::max<float>(ft, 0); }
    float fuzziness() const { return _fuzziness; }

protected:
    glm::ivec3 _size;
    float _fuzziness;
    std::vector<IVolumeSampler*> _additiveSamplers, _subtractiveSamplers;
    std::vector<std::unique_ptr<IVolumeSampler>> _samplers;
};

class OctreeVolume : public BaseCompositeVolume {
public:
    struct Node {
        Node() = delete;
        Node(const Node&) = delete;
        Node(const Node&&) = delete;
        Node(const util::AABB bounds, int depth, int childIdx)
            : bounds(bounds)
            , depth(depth)
            , childIdx(childIdx)
        {
        }
        ~Node() = default;

        util::AABB bounds;
        int depth = 0;
        int childIdx = 0;
        bool isLeaf = false;
        bool march = false;
        bool empty = false;
        std::array<std::unique_ptr<Node>, 8> children;
        std::unordered_set<IVolumeSampler*> additiveSamplers;
        std::unordered_set<IVolumeSampler*> subtractiveSamplers;

    private:
        friend class OctreeVolume;

        std::vector<IVolumeSampler*> _additiveSamplersVec;
        std::vector<IVolumeSampler*> _subtractiveSamplersVec;
    };

public:
    OctreeVolume(int size, float fuzziness, int minNodeSize,
        const std::shared_ptr<util::ThreadPool> threadPool,
        const std::vector<util::unowned_ptr<TriangleConsumer<Vertex>>>& triangleConsumers)
        : BaseCompositeVolume(glm::ivec3 { size, size, size }, fuzziness)
        , _bounds(util::AABB(glm::ivec3(0, 0, 0), glm::ivec3(size, size, size)))
        , _root(buildOctreeNode(_bounds, minNodeSize, 0, 0))
        , _threadPool(threadPool)
        , _triangleConsumers(triangleConsumers)
    {
    }

    void collect(std::vector<Node*>& collector)
    {
        /*
            TODO: I believe I should probably skip the root node; it will always intersect
            something; it's not useful.
        */
        mark(_root.get());
        collect(_root.get(), collector);
    }

    void walkOctree(std::function<bool(Node*)> visitor)
    {
        std::function<void(Node*)> walker = [&visitor, &walker](Node* node) {
            if (visitor(node)) {
                if (!node->isLeaf) {
                    for (auto& child : node->children) {
                        walker(child.get());
                    }
                }
            }
        };

        walker(_root.get());
    }

    /**
     * March the represented volume into the triangle consumers provided in the constructor
    */
    void march(std::function<void(OctreeVolume::Node*)> marchedNodeObserver = nullptr);

    /**
     * March the volume in a non-blocking fashion; calls onReady on the
     * main thread when the work is done
     * NOTE:
     * onReady will be called on the main thread, which requires use of
     *  mc::util::MainThreadQueue
     * marchedNodeObserver will be called on the calling thread
     */
    void marchAsync(
        std::function<void()> onReady,
        std::function<void(OctreeVolume::Node*)> marchedNodeObserver = nullptr);

    /**
     * Get the bounds of this volume - no geometry will exceed this region
     */
    util::AABB bounds() const
    {
        return _bounds;
    }

    /**
     * Get the max octree node depth
     */
    size_t depth() const
    {
        return _treeDepth;
    }

protected:
    void marchSetup(std::function<void(OctreeVolume::Node*)> marchedNodeObserver);
    std::vector<std::future<void>> marchCollectedNodes();
    void marchNode(OctreeVolume::Node* node, TriangleConsumer<Vertex>& tc);

    /**
     * Mark the nodes which should be marched.
    */
    bool mark(Node* currentNode) const
    {
        currentNode->empty = true;
        currentNode->march = false;

        currentNode->additiveSamplers.clear();
        currentNode->subtractiveSamplers.clear();

        for (const auto sampler : _additiveSamplers) {
            if (sampler->intersects(currentNode->bounds)) {
                currentNode->additiveSamplers.insert(sampler);
                currentNode->empty = false;
            }
        }

        // we don't care about subtractiveSamplers UNLESS the
        // current node has additive ones; because without any
        // additive samplers, there is no volume to subtract from
        if (!currentNode->empty) {
            for (const auto sampler : _subtractiveSamplers) {
                auto intersection = sampler->intersection(currentNode->bounds);
                switch (intersection) {
                case IVolumeSampler::AABBIntersection::IntersectsAABB:
                    currentNode->subtractiveSamplers.insert(sampler);
                    break;
                case IVolumeSampler::AABBIntersection::ContainsAABB:
                    // special case - this node is completely contained
                    // by the volume, which means it is EMPTY.
                    currentNode->additiveSamplers.clear();
                    currentNode->subtractiveSamplers.clear();
                    currentNode->empty = true;
                    break;
                case IVolumeSampler::AABBIntersection::None:
                    break;
                }

                // if a subtractive sampler cleared this node,
                // break the loop, we're done
                if (currentNode->empty) {
                    break;
                }
            }
        }

        if (!currentNode->empty) {

            if (currentNode->isLeaf) {
                currentNode->march = true;
                return true;
            }

            // some samplers intersect this node; traverse down
            int occupied = 0;
            for (auto& child : currentNode->children) {
                if (mark(child.get())) {
                    occupied++;
                }
            }

            if (occupied == 8) {

                // all 8 children intersect samplers; mark self to march, and
                // coalesce their samplers into currentNode
                currentNode->march = true;

                // copy up their samplers
                for (auto& child : currentNode->children) {
                    child->march = false;
                    currentNode->additiveSamplers.insert(std::begin(child->additiveSamplers), std::end(child->additiveSamplers));
                    currentNode->subtractiveSamplers.insert(std::begin(child->subtractiveSamplers), std::end(child->subtractiveSamplers));
                }

                return true;
            }
        }

        return false;
    }

    /**
     * After calling mark(), this will collect all nodes which should be marched
    */
    void collect(Node* currentNode, std::vector<Node*>& nodesToMarch) const
    {
        if (currentNode->empty)
            return;

        if (currentNode->march) {
            // collect this node, don't recurse further
            nodesToMarch.push_back(currentNode);
        } else if (!currentNode->isLeaf) {
            // if this is not a leaf node and not marked
            for (const auto& child : currentNode->children) {
                collect(child.get(), nodesToMarch);
            }
        }
    }

    std::unique_ptr<Node> buildOctreeNode(util::AABB bounds, size_t minNodeSize, size_t depth, size_t childIdx)
    {
        _treeDepth = std::max(depth, _treeDepth);
        auto node = std::make_unique<Node>(bounds, depth, childIdx);

        // we're working on cubes, so only one bounds size is checked
        size_t size = bounds.size().x;
        if (size / 2 >= minNodeSize) {
            node->isLeaf = false;
            auto childBounds = bounds.octreeSubdivide();
            for (size_t i = 0, N = childBounds.size(); i < N; i++) {
                node->children[i] = buildOctreeNode(childBounds[i], minNodeSize, depth + 1, i);
            }
        } else {
            node->isLeaf = true;
        }

        return node;
    }

private:
    util::AABB _bounds;
    size_t _treeDepth = 0;
    std::unique_ptr<Node> _root;
    std::vector<Node*> _nodesToMarch;
    std::shared_ptr<util::ThreadPool> _threadPool;
    std::vector<util::unowned_ptr<TriangleConsumer<Vertex>>> _triangleConsumers;
    std::size_t _asyncMarchId { 0 };
    std::mutex _queuePopMutex;
    std::future<void> _asyncMarchWaiter;
};

} // namespace mc

#endif /* volume_hpp */
