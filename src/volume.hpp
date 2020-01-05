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
#include <unordered_set>
#include <vector>

#include "aabb.hpp"
#include "marching_cubes.hpp"
#include "unowned_ptr.hpp"

#pragma mark - VolumeSampler

class IVolumeSampler {
public:
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
    virtual bool test(const AABB bounds) const = 0;

    /*
     Return the amount a given point in space is "inside" the volume. The
     fuzziness is a gradient to allow for smoothing. For
     example, in the case of a circle or radius r, a point distance <= r - fuziness
     returns a value of 1, and a distance >= r returns 0,
     and values in between return a linear transition.
     */
    virtual float valueAt(const vec3& p, float fuzziness) const = 0;

private:
    Mode _mode;
};

#pragma mark - Volume

class BaseCompositeVolume : public mc::IIsoSurface {
public:
    BaseCompositeVolume(ivec3 size, float falloffThreshold)
        : _size(size)
        , _fuzziness(falloffThreshold)
    {
    }

    template <typename T>
    unowned_ptr<T> add(std::unique_ptr<T>&& sampler)
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

        onVolumeSamplerAdded(sPtr);

        return sPtr;
    }

    ivec3 size() const override
    {
        return _size;
    }

    float valueAt(const vec3& p) const override
    {
        // TODO(shamyl@gmail.com): Adapt the smooth-min functions; they're meant
        // for SDF; but the idea is applicable here.
        //        float v = 0;
        //        for (auto& s : _additiveSamplers) {
        //            v = smin(v, s->valueAt(p, _falloffThreshold), 10.0f);
        //        }
        //
        //        for (auto& s : _subtractiveSamplers) {
        //            v = smin(v, -s->valueAt(p, _falloffThreshold), 10.0f);
        //        }

        float v = 0;
        for (auto& s : _additiveSamplers) {
            v += s->valueAt(p, _fuzziness);
        }

        for (auto& s : _subtractiveSamplers) {
            v -= s->valueAt(p, _fuzziness);
        }

        return min(max(v, 0.0F), 1.0F);
    }

    void setFuzziness(float ft) { _fuzziness = max<float>(ft, 0); }
    float fuzziness() const { return _fuzziness; }

protected:
    // polynomial smooth min
    // https://www.iquilezles.org/www/articles/smin/smin.htm
    static inline float smin(float a, float b, float k)
    {
        float h = std::max<float>(k - abs(a - b), 0.0f) / k;
        return std::min<float>(a, b) - h * h * k * (1.0f / 4.0f);
    }

    // cubic smooth min
    // https://www.iquilezles.org/www/articles/smin/smin.htm
    static inline float sminCubic(float a, float b, float k)
    {
        float h = std::max<float>(k - abs(a - b), 0.0f) / k;
        return std::min<float>(a, b) - h * h * h * k * (1.0f / 6.0f);
    }

    virtual void onVolumeSamplerAdded(IVolumeSampler* sampler) {}

protected:
    ivec3 _size;
    float _fuzziness;
    std::vector<IVolumeSampler*> _additiveSamplers, _subtractiveSamplers;
    std::vector<std::unique_ptr<IVolumeSampler>> _samplers;
};

class OctreeVolume : public BaseCompositeVolume {
public:
    struct Node {
        Node(const iAABB bounds, int depth)
            : bounds(bounds)
            , depth(depth)
        {
        }

        iAABB bounds;
        int depth;
        bool isLeaf = false;
        std::array<std::unique_ptr<Node>, 8> children;
        std::unordered_set<IVolumeSampler*> additiveSamplers;
        std::unordered_set<IVolumeSampler*> subtractiveSamplers;
    };

public:
    OctreeVolume(int size, float falloffThreshold, int minNodeSize)
        : BaseCompositeVolume(ivec3 { size, size, size }, falloffThreshold)
        , _root(buildOctreeNode(iAABB(ivec3(0, 0, 0), ivec3(size, size, size)), minNodeSize, 0))
    {
    }

    void visitOccupiedNodes(bool update, std::function<void(Node*)> visitor)
    {
        if (update) {
            // find the nodes which should be marched
            _nodesToMarch.clear();
            collect(_root.get(), _nodesToMarch);
        }

        for (auto node : _nodesToMarch) {
            visitor(node);
        }
    }

    void walkOctree(std::function<void(Node*)> visitor)
    {
        std::function<void(Node*)> walker = [&visitor, &walker](Node* node) {
            visitor(node);
            if (!node->isLeaf) {
                for (auto& child : node->children) {
                    walker(child.get());
                }
            }
        };

        walker(_root.get());
    }

protected:
    bool collect(Node* currentNode, std::vector<Node*>& nodesToMarch) const {
        /*
         NOTE: I believe I should probably skip the root node; it will always intersect
         something; it's not useful. Perhaps we should just manually iterate its 8 children?
         */

        currentNode->additiveSamplers.clear();
        currentNode->subtractiveSamplers.clear();

        bool intersectSamplers = false;
        for (const auto sampler : _additiveSamplers) {
            if (sampler->test(currentNode->bounds)) {
                currentNode->additiveSamplers.insert(sampler);
                intersectSamplers = true;
            }
        }

        for (const auto sampler : _subtractiveSamplers) {
            if (sampler->test(currentNode->bounds)) {
                currentNode->subtractiveSamplers.insert(sampler);
                intersectSamplers = true;
            }
        }
        
        if (intersectSamplers) {
            if (currentNode->isLeaf) {
                // currentNode is a leaf node and intersects samplers; so add it to march list
                nodesToMarch.push_back(currentNode);
            } else {

                // some samplers intersect this node; traverse down
                int occupied = 0;
                for (auto& child : currentNode->children) {
                    if (collect(child.get(), nodesToMarch)) {
                        occupied++;
                    }
                }
                
                if (occupied == 8) {
                    // all 8 children intersect samplers; we can just coalesce their
                    // samplers into currentNode, and discard children from the list to march
                    
                    for (int i = 0; i < 8; i++) {
                        nodesToMarch.pop_back();
                    }

                    nodesToMarch.push_back(currentNode);

                    // copy up their samplers
                    for (auto& child : currentNode->children) {
                        currentNode->additiveSamplers.insert(std::begin(child->additiveSamplers), std::end(child->additiveSamplers));
                        currentNode->subtractiveSamplers.insert(std::begin(child->subtractiveSamplers), std::end(child->subtractiveSamplers));
                    }
                }
            }
            
            return true;
        }
            
        return false;
    }

    static std::unique_ptr<Node> buildOctreeNode(iAABB bounds, int minNodeSize, int depth)
    {
        auto node = std::make_unique<Node>(bounds, depth);

        // we're working on cubes, so only one bounds size is checked
        int size = bounds.size().x;
        if (size / 2 >= minNodeSize) {
            node->isLeaf = false;
            auto childBounds = octreeSubdivide(bounds);
            for (size_t i = 0, N = childBounds.size(); i < N; i++) {
                node->children[i] = buildOctreeNode(childBounds[i], minNodeSize, depth + 1);
            }
        } else {
            node->isLeaf = true;
        }

        return node;
    }

    static std::array<iAABB, 8> octreeSubdivide(const iAABB bounds)
    {
        auto center = bounds.center();
        int s = bounds.size().x / 2;
        int hs = s / 2;
        return std::array<iAABB, 8> {
            iAABB(center + ivec3(-hs, -hs, -hs), hs),
            iAABB(center + ivec3(+hs, -hs, -hs), hs),
            iAABB(center + ivec3(+hs, -hs, +hs), hs),
            iAABB(center + ivec3(-hs, -hs, +hs), hs),
            iAABB(center + ivec3(-hs, +hs, -hs), hs),
            iAABB(center + ivec3(+hs, +hs, -hs), hs),
            iAABB(center + ivec3(+hs, +hs, +hs), hs),
            iAABB(center + ivec3(-hs, +hs, +hs), hs)
        };
    }

private:
    std::unique_ptr<Node> _root;
    std::vector<Node*> _nodesToMarch;
};

#endif /* volume_hpp */
