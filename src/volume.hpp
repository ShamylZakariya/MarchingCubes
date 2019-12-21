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

private:
    ivec3 _size;
    float _fuzziness;
    std::vector<IVolumeSampler*> _additiveSamplers, _subtractiveSamplers;
    std::vector<std::unique_ptr<IVolumeSampler>> _samplers;
};

class OctreeVolume : public BaseCompositeVolume {
public:
    OctreeVolume(int size, float falloffThreshold, int minNodeSize)
        : BaseCompositeVolume(ivec3 { size, size, size }, falloffThreshold)
        , _root(buildOctreeNode(iAABB(ivec3(0, 0, 0), ivec3(size, size, size)), minNodeSize, 0))
    {
    }

    void update()
    {
        // perform a postorder traversal; starting with leaves, for each
        // leaf set its march to true iff it intersects a sampler; then
        // on the up-pass, for each node, set march == all-8-children march == true
    }

protected:
    struct Node {
        Node(const iAABB bounds, int depth)
            : bounds(bounds)
            , depth(depth)
        {
        }

        iAABB bounds;
        int depth;
        bool march = false;
        std::array<std::unique_ptr<Node>, 8> children;
    };

    static std::unique_ptr<Node> buildOctreeNode(iAABB bounds, int minNodeSize, int depth)
    {
        auto node = std::make_unique<Node>(bounds, depth);

        // we're working on cubes, so only one bounds size is checked
        int size = bounds.size().x;
        if (size / 2 >= minNodeSize) {
            auto childBounds = octreeSubdivide(bounds);
            for (size_t i = 0, N = childBounds.size(); i < N; i++) {
                node->children[i] = buildOctreeNode(childBounds[i], minNodeSize, depth + 1);
            }
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
};

#endif /* volume_hpp */
