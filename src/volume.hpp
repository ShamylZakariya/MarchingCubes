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

    virtual AABB bounds() const = 0;
    virtual float valueAt(const vec3& p, float falloffThreshold) const = 0;

private:
    Mode _mode;
};

#pragma mark - Volume

class BaseCompositeVolume : public mc::IIsoSurface {
public:
    BaseCompositeVolume(ivec3 size, float falloffThreshold)
        : _size(size)
        , _falloffThreshold(falloffThreshold)
    {
    }

    template <typename T>
    unowned_ptr<T> add(std::unique_ptr<T>&& sampler)
    {
        auto ret = sampler.get();
        switch (sampler->mode()) {
        case IVolumeSampler::Mode::Additive:
            _additiveSamplers.push_back(std::move(sampler));
            break;

        case IVolumeSampler::Mode::Subtractive:
            _subtractiveSamplers.push_back(std::move(sampler));
            break;
        }
        onVolumeSamplerAdded(ret);
        return ret;
    }

    ivec3 size() const override
    {
        return _size;
    }

    float valueAt(const vec3& p) const override
    {
        float v = 0;
        for (auto& s : _additiveSamplers) {
            v += s->valueAt(p, _falloffThreshold);
        }

        for (auto& s : _subtractiveSamplers) {
            v -= s->valueAt(p, _falloffThreshold);
        }

        return min(max(v, 0.0F), 1.0F);
    }

    void setFalloffThreshold(float ft) { _falloffThreshold = max<float>(ft, 0); }
    float falloffThreshold() const { return _falloffThreshold; }

protected:
    virtual void onVolumeSamplerAdded(IVolumeSampler* sampler) {}

private:
    ivec3 _size;
    float _falloffThreshold;
    std::vector<std::unique_ptr<IVolumeSampler>> _additiveSamplers, _subtractiveSamplers;
};

class OctreeVolume : public BaseCompositeVolume {
public:
    OctreeVolume(int size, float falloffThreshold, int minNodeSize)
        : BaseCompositeVolume(ivec3 { size, size, size }, falloffThreshold)
        , _root(buildOctreeNode(iAABB(ivec3(0, 0, 0), ivec3(size, size, size)), minNodeSize, 0))
    {
    }

protected:
    struct Node {
        Node(const iAABB bounds, int depth)
            : bounds(bounds), depth(depth)
        {
        }

        iAABB bounds;
        int depth;
        std::vector<IVolumeSampler*> additiveSamplers;
        std::vector<IVolumeSampler*> subtractiveSamplers;
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
                node->children[i] = buildOctreeNode(childBounds[i], minNodeSize, depth+1);
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
