//
//  volume.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/27/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef volume_hpp
#define volume_hpp

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

class Volume : public mc::IIsoSurface {
public:
    Volume(ivec3 size, float falloffThreshold)
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

private:
    ivec3 _size;
    float _falloffThreshold;
    std::vector<std::unique_ptr<IVolumeSampler>> _additiveSamplers, _subtractiveSamplers;
};

#endif /* volume_hpp */
