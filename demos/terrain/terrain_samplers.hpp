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

class Tube : public mc::IVolumeSampler {
private:

    vec3 _tubeAxisOrigin;
    vec3 _tubeAxisDir;
    float _innerRadius;
    float _outerRadius;
    float _innerRadius2;
    float _outerRadius2;

    vec3 _frontFaceNormal;
    vec3 _frontFaceOrigin;
    vec3 _backFaceNormal;
    vec3 _backFaceOrigin;


public:
    Tube() = delete;

    /**
     * Create a tube with perpendicular end caps
     * tubeAxisOrigin: center of cylinder
     * tubeAxisDir: +dir of the cylinder axis
     * innerRadius: inside radius of tube
     * outerRadius: outside radius of tube
     * length: length of tube
     */
    Tube(vec3 tubeAxisOrigin, vec3 tubeAxisDir, float innerRadius, float outerRadius, float length)
        : IVolumeSampler(Mode::Additive)
        , _tubeAxisOrigin(tubeAxisOrigin)
        , _tubeAxisDir(normalize(tubeAxisDir))
        , _innerRadius(innerRadius)
        , _outerRadius(outerRadius)
        , _innerRadius2(innerRadius*innerRadius)
        , _outerRadius2(outerRadius*outerRadius)
        , _frontFaceNormal(_tubeAxisDir)
        , _frontFaceOrigin(_tubeAxisOrigin + _tubeAxisDir * (length / 2))
        , _backFaceNormal(_tubeAxisDir)
        , _backFaceOrigin(_tubeAxisOrigin - _tubeAxisDir * (length / 2))
    {
    }

    /**
     * Create a tube with angled caps
     * tubeAxisOrigin: center of cylinder
     * tubeAxisDir: +dir of the cylinder axis
     * innerRadius: inside radius of tube
     * outerRadius: outside radius of tube
     * length: length of tube
     * frontFaceNormal: normal of the +dir end cap
     * backFaceNormal: normal of the -dir end cap
     */
    Tube(vec3 tubeAxisOrigin, vec3 tubeAxisDir, float innerRadius, float outerRadius, float length, vec3 frontFaceNormal, vec3 backFaceNormal)
        : IVolumeSampler(Mode::Additive)
        , _tubeAxisOrigin(tubeAxisOrigin)
        , _tubeAxisDir(normalize(tubeAxisDir))
        , _innerRadius(innerRadius)
        , _outerRadius(outerRadius)
        , _innerRadius2(innerRadius*innerRadius)
        , _outerRadius2(outerRadius*outerRadius)
        , _frontFaceNormal(frontFaceNormal)
        , _frontFaceOrigin(_tubeAxisOrigin + _tubeAxisDir * (length / 2))
        , _backFaceNormal(backFaceNormal)
        , _backFaceOrigin(_tubeAxisOrigin - _tubeAxisDir * (length / 2))
    {
    }

    Tube(const Tube&) = delete;
    Tube(Tube&&) = delete;
    ~Tube() = default;

    bool intersects(AABB bounds) const override
    {
        const auto corners = bounds.corners();

        // quick check on our bounding planes
        if (mc::volume_samplers_helpers::boundedSpaceIntersection(
                _frontFaceOrigin, _frontFaceNormal, _backFaceOrigin, _backFaceNormal,
                corners)
            == AABBIntersection::None) {
            return false;
        }

        // find the closest and farthest vertices on the bounds to our
        // axis and discard if the closest is beyond the cylinder outer
        // radius, or if the farthest is inside the inner radius
        float closestDist2 = std::numeric_limits<float>::max();
        float farthestDist2 = 0;

        for (const auto& c : corners) {
            float d2 = _distanceToAxis2(c);
            closestDist2 = min(d2, closestDist2);
            farthestDist2 = max(d2, farthestDist2);
        }

        if (closestDist2 > _outerRadius2) {
            return false;
        }

        if (farthestDist2 < _innerRadius2) {
            return false;
        }

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
        // - On negative side front and back planes
        // - Between inner and outer radius from axis
        float radialDist2 = _distanceToAxis2(p);

        if (radialDist2 > _outerRadius2 || radialDist2 < _innerRadius2) {
            return 0;
        }

        float frontFaceDist = dot(_frontFaceNormal, p - _frontFaceOrigin);
        float backFaceDist = dot(_backFaceNormal, p - _backFaceOrigin);

        if (frontFaceDist > 0 || backFaceDist > 0) {
            return 0;
        }

        float planeBoundsContribution = min<float>(-frontFaceDist / fuzziness, 1);
        planeBoundsContribution += min<float>(-frontFaceDist / fuzziness, 1);
        planeBoundsContribution = min<float>(planeBoundsContribution, 1);

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
        // compute four normals for this point:
        // - negative face normal
        // - positive face normal
        // - outer cylinder normal
        // - inner cylinder normal
        // and the respective distances from each surface / fuzziness
        // then scale each normal accordingly, and return the
        // normalization of the sum

        float frontFaceDist = dot(_frontFaceNormal, p - _frontFaceOrigin);
        float backFaceDist = dot(_backFaceNormal, p - _backFaceOrigin);
        vec3 pointOnAxis;
        float radialDist2 = _distanceToAxis2(p, pointOnAxis);

        // early exit
        if (radialDist2 > _outerRadius2 || radialDist2 < _innerRadius2
            || frontFaceDist > 0 || backFaceDist > 0) {
            return vec3 { 0 };
        }

        return _tubeAxisDir;

        // TODO: Implement meaningful normal, because this isn't working
        // // distance of this point from the surface of pos and neg planes
        // const auto posPlaneDist = abs(axisDist - _halfWidth);
        // const auto negPlaneDist = abs(-axisDist - _halfWidth);
        // const auto posPlaneContribution = 1 - min<float>(posPlaneDist / fuzziness, 1);
        // const auto negPlaneContribution = 1 - min<float>(negPlaneDist / fuzziness, 1);

        // const auto radialDist = sqrt(radialDist2);
        // const auto outerRingDist = max(radialDist - _outerRadius, 0.0F);
        // const auto innerRingDist = max(_innerRadius - radialDist, 0.0F);
        // const auto outerRingContribution = 1 - min<float>(outerRingDist / fuzziness, 1);
        // const auto innerRingContribution = 1 - min<float>(innerRingDist / fuzziness, 1);

        // const vec3 outerRingNormal = normalize(p - pointOnAxis);

        // return normalize(
        //     (_cylinderAxisDirection * posPlaneContribution)
        //     + (-_cylinderAxisDirection * negPlaneContribution)
        //     + (outerRingNormal * outerRingContribution)
        //     + (-outerRingNormal * innerRingContribution)
        // );
    }

private:
    //http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
    // vec3 a = _cylinderAxisOrigin;
    // vec3 b = _cylinderAxisOrigin + _cylinderAxisDirection;
    // float d = dot((a - p), (b - a));
    // float t = -d / length2(b - a);
    // vec3 tp = mix(a,b,t);
    // return distance(tp, p);

    inline float _distanceToAxis(vec3 p) const
    {
        float t = -dot(_tubeAxisOrigin - p, _tubeAxisDir);
        auto tp = _tubeAxisOrigin + t * _tubeAxisDir;
        return distance(tp, p);
    }

    inline float _distanceToAxis2(vec3 p) const
    {
        float t = -dot(_tubeAxisOrigin - p, _tubeAxisDir);
        auto tp = _tubeAxisOrigin + t * _tubeAxisDir;
        return distance2(tp, p);
    }

    inline float _distanceToAxis2(vec3 p, vec3& pointOnAxis) const
    {
        float t = -dot(_tubeAxisOrigin - p, _tubeAxisDir);
        pointOnAxis = _tubeAxisOrigin + t * _tubeAxisDir;
        return distance2(pointOnAxis, p);
    }
};

#endif