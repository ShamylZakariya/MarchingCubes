#ifndef terrain_samplers_hpp
#define terrain_samplers_hpp

#include <memory>

#include <epoxy/gl.h>

#include <mc/util/unowned_ptr.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "perlin_noise.hpp"

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;

class GroundSampler : public mc::IVolumeSampler {
public:
    typedef std::function<float(vec3)> NoiseSampler;

public:
    GroundSampler() = delete;
    GroundSampler(const GroundSampler&) = delete;
    GroundSampler(GroundSampler&&) = delete;

    GroundSampler(NoiseSampler noise, float height, bool smooth)
        : IVolumeSampler(IVolumeSampler::Mode::Additive)
        , _noise(noise)
        , _height(height)
        , _smooth(smooth)
    {
    }

    ~GroundSampler() = default;

    void setSampleOffset(vec3 offset)
    {
        _sampleOffset = offset;
    }

    vec3 sampleOffset() const
    {
        return _sampleOffset;
    }

    void setSmooth(bool smooth) { _smooth = smooth; }
    bool smooth() const { return _smooth; }

    bool intersects(AABB bounds) const override
    {
        // We know that the geometry will span [x,y] and be no taller than
        // the _height, so we can do an easy test
        // TODO: query the perlin function?
        return bounds.min.y <= _height;
    }

    AABBIntersection intersection(AABB bounds) const override
    {
        // This is only meaningful for subtractive samplers
        // so it will suffice to provide a stub
        if (bounds.min.y > _height) {
            return AABBIntersection::None;
        } else if (bounds.max.y < _height) {
            return AABBIntersection::ContainsAABB;
        }
        return AABBIntersection::IntersectsAABB;
    }

    float valueAt(const vec3& p, float fuzziness) const override
    {
        auto y = _sample(p);
        auto innerY = y - fuzziness;
        if (p.y > y) {
            return 0;
        } else if (p.y < innerY) {
            return 1;
        }

        return 1 - ((p.y - innerY) / fuzziness);
    }

    vec3 normalAt(const vec3& p, float fuzziness) const override
    {
        if (_smooth) {
            const float d = 0.75f;
            vec3 grad(
                _sample(p + vec3(d, 0, 0)) - _sample(p + vec3(-d, 0, 0)),
                _sample(p + vec3(0, d, 0)) - _sample(p + vec3(0, -d, 0)),
                _sample(p + vec3(0, 0, d)) - _sample(p + vec3(0, 0, -d)));

            return -normalize(grad);
        }
        return vec3 { 0, -1, 0 };
    }

private:
    inline float _sample(const vec3& p) const
    {
        float n = _noise(p + _sampleOffset);

        // sand-dune like structure
        float dune = n * 5;
        dune = dune - floor(dune);
        dune = dune * dune * _height;

        // floor
        float f = n * n * n;
        float floor = f * _height;

        return floor + dune;
    }

    NoiseSampler _noise;
    float _height { 0 };
    vec3 _sampleOffset { 0 };
    bool _smooth = true;
};

#endif