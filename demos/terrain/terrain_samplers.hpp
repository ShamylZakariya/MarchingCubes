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
        throw std::runtime_error("intersection only meanignful for subtractive volumes");
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

class Ring : public mc::IVolumeSampler {
public:
    Ring() = delete;
    Ring(vec3 axisOrigin, vec3 axisDirection, float halfWidth, float innerRadius, float outerRadius)
        : IVolumeSampler(Mode::Additive)
        , _axisOrigin(axisOrigin)
        , _axisDirection(normalize(axisDirection))
        , _halfWidth(halfWidth)
        , _innerRadius(innerRadius)
        , _outerRadius(outerRadius)
    {
        _innerRadius2 = _innerRadius * _innerRadius;
        _outerRadius2 = _outerRadius * _outerRadius;
    }

    Ring(const Ring&) = delete;
    Ring(Ring&&) = delete;
    ~Ring() = default;

    bool intersects(AABB bounds) const override
    {
        // quick check on our bounding planes
        if (mc::volume_samplers_detail::boundedPlaneIntersection(_axisOrigin, _axisDirection, _halfWidth, bounds) == AABBIntersection::None) {
            return false;
        }

        // find the point on surface of bounds closest to _position
        const auto closestPoint = glm::vec3 {
            max(bounds.min.x, min(_axisOrigin.x, bounds.max.x)),
            max(bounds.min.y, min(_axisOrigin.y, bounds.max.y)),
            max(bounds.min.z, min(_axisOrigin.z, bounds.max.z))
        };

        if (distance2(closestPoint, _axisOrigin) > _outerRadius2) {
            return false;
        }

        // TODO: Implement radial bounds testing
        return true;
    }

    AABBIntersection intersection(AABB bounds) const override
    {
        // This is only meaningful for subtractive samplers
        throw std::runtime_error("intersection only meanignful for subtractive volumes");
    }

    float valueAt(const vec3& p, float fuzziness) const override
    {
        // A point is inside the ring volume if:
        // - On negative side of both bounding planes
        // - Between inner and outer radius from axis
        float axisDist = abs(dot(_axisDirection, p - _axisOrigin));
        float radialDist2 = _distanceToAxis2(p);

        if (radialDist2 > _outerRadius2 || radialDist2 < _innerRadius2) {
            return 0;
        }

        // if (radialDist2 > (_outerRadius*_outerRadius) || radialDist2< (_innerRadius*_innerRadius)) {
        //     return 0;
        // }

        // if (axisDist > _halfWidth) {
        //     return 0;
        // }

        // return 1;

        float innerDist = _halfWidth - fuzziness;
        float planeBoundsContribution = 1;

        if (axisDist >= _halfWidth) {
            return 0;
        } else if (axisDist > innerDist) {
            planeBoundsContribution = 1 - ((axisDist - innerDist) / fuzziness);
        }

        float tubeContribution = 1;
        float outerRadiusInner = _outerRadius - fuzziness;
        float innerRadiusInner = _innerRadius + fuzziness;
        float outerRadiusInner2 = outerRadiusInner * outerRadiusInner;
        float innerRadiusInner2 = innerRadiusInner * innerRadiusInner;

        if (radialDist2 < innerRadiusInner2) {
            // gradient on inner
            float radialDist = sqrt(radialDist2);
            tubeContribution = (radialDist - _innerRadius) / fuzziness;
        } else if (radialDist2 > outerRadiusInner2) {
            // gradient on outer
            float radialDist = sqrt(radialDist2);
            tubeContribution = 1 - ((radialDist - outerRadiusInner) / fuzziness);
        }

        return planeBoundsContribution * tubeContribution;
    }

    vec3 normalAt(const vec3& p, float fuzziness) const override
    {
        // TODO: Implement normal
        return vec3(0, 1, 0);
    }

private:
    inline float _distanceToAxis(vec3 p) const
    {
        //http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
        // vec3 a = _axisOrigin;
        // vec3 b = _axisOrigin + _axisDirection;
        // float d = dot((a - p), (b - a));
        // float t = -d / length2(b - a);
        // vec3 tp = mix(a,b,t);
        // return distance(tp, p);

        // simplifies to:
        float t = -dot(_axisOrigin - p, _axisDirection);
        auto tp = _axisOrigin + t * _axisDirection;
        return distance(tp, p);
    }

    inline float _distanceToAxis2(vec3 p) const
    {
        //http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
        // vec3 a = _axisOrigin;
        // vec3 b = _axisOrigin + _axisDirection;
        // float d = dot((a - p), (b - a));
        // float t = -d / length2(b - a);
        // vec3 tp = mix(a,b,t);
        // return distance2(tp, p);

        // simplifies to:
        float t = -dot(_axisOrigin - p, _axisDirection);
        auto tp = _axisOrigin + t * _axisDirection;
        return distance2(tp, p);
    }

private:
    vec3 _axisOrigin;
    vec3 _axisDirection;
    float _halfWidth;
    float _innerRadius;
    float _outerRadius;
    float _innerRadius2;
    float _outerRadius2;
};

#endif