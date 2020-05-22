#ifndef terrain_samplers_hpp
#define terrain_samplers_hpp

#include <memory>

#include <epoxy/gl.h>

#include <mc/util/unowned_ptr.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;

class GroundSampler : public mc::IVolumeSampler {
public:
    typedef std::function<float(const vec3&)> NoiseSampler3D;

public:
    GroundSampler() = delete;
    GroundSampler(const GroundSampler&) = delete;
    GroundSampler(GroundSampler&&) = delete;

    GroundSampler(NoiseSampler3D noise3D, float height, float floorThreshold,
        const mc::MaterialState& floorMaterial,
        const mc::MaterialState& lowMaterial,
        const mc::MaterialState& highMaterial)
        : IVolumeSampler(IVolumeSampler::Mode::Additive)
        , _noise3D(noise3D)
        , _height(height)
        , _floorPinion(floorThreshold / height)
        , _floorMaterial(floorMaterial)
        , _lowMaterial(lowMaterial)
        , _highMaterial(highMaterial)
    {
    }

    ~GroundSampler() = default;

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

    float valueAt(const vec3& p, float fuzziness, mc::MaterialState& material) const override
    {
        float sampleHeight = clamp(p.y / _height, 0.0F, 1.0F);

        if (sampleHeight < _floorPinion) {
            material = _floorMaterial;
        } else {
            float t = (sampleHeight - _floorPinion) / (1 - _floorPinion);
            t = (1 - sampleHeight);
            material = mix(_highMaterial, _lowMaterial, t * t * t);
        }

        if (p.y < 1e-3) {
            return 1;
        }

        float n = _noise3D(p);
        float height = glm::clamp((p.y / _height), 0.0F, 1.0F);
        float falloff = 1 - (height * height * height);
        n *= falloff;
        return n;
    }

private:
    NoiseSampler3D _noise3D;
    float _height { 0 };
    float _floorPinion { 0 };

    mc::MaterialState _floorMaterial, _lowMaterial, _highMaterial;
};

class Tube : public mc::IVolumeSampler {
private:
    vec3 _tubeAxisOrigin { 0 };
    vec3 _tubeAxisDir { 0, 0, 1 };
    vec3 _tubeAxisPerp { 0, 1, 0 };
    vec3 _innerRadiusOffset { 0 };
    float _innerRadius { 0 };
    float _outerRadius { 0 };
    float _innerRadius2 { 0 };
    float _outerRadius2 { 0 };

    vec3 _frontFaceNormal { 0, 0, 1 };
    vec3 _frontFaceOrigin { 0 };
    vec3 _backFaceNormal { 0, 0, -1 };
    vec3 _backFaceOrigin { 0, 0, 0 };

    float _cutAngleRadians { 0 };
    float _cosCutAngle { 0 };
    bool _hasInnerCylinderOffset = false;
    mc::MaterialState _material;

public:
    struct Config {
        // origin of the cylinder representing the outer radius of the tube
        vec3 axisOrigin { 0 };
        // major axis of the cylinder making the outer radius of the tube
        vec3 axisDir { 0, 0, 1 };

        // perpendicular to the axisDir, used for computing the cut angle
        vec3 axisPerp { 0, 1, 0 };

        // offset of the cylinder making up the inner radius from axisOrigin
        // if 0, both cylinders are coaxial, but an offset can produce interesting
        // non-symmetries
        vec3 innerRadiusAxisOffset { 0 };

        // inner radius of tube
        float innerRadius = 0;

        // outer radius of tube
        float outerRadius = 0;

        // length of tube (end to end)
        float length = 1;

        // [0,2*pi] cuts a notch our of the tube lengthwise with center of
        // notch aligned via axisPerp
        float cutAngleRadians = 0;

        // normal of the front capping plane of the tube
        vec3 frontFaceNormal { 0, 0, 1 };

        // normal of the back capping plane of the tube
        vec3 backFaceNormal { 0, 0, -1 };

        // material tubes will emit
        mc::MaterialState material;
    };

    Tube() = delete;

    Tube(Config c)
        : IVolumeSampler(Mode::Additive)
        , _tubeAxisOrigin(c.axisOrigin)
        , _tubeAxisDir(normalize(c.axisDir))
        , _tubeAxisPerp(normalize(c.axisPerp))
        , _innerRadiusOffset(c.innerRadiusAxisOffset)
        , _innerRadius(c.innerRadius)
        , _outerRadius(c.outerRadius)
        , _innerRadius2(c.innerRadius * c.innerRadius)
        , _outerRadius2(c.outerRadius * c.outerRadius)
        , _frontFaceNormal(normalize(c.frontFaceNormal))
        , _frontFaceOrigin(_tubeAxisOrigin + _tubeAxisDir * (c.length / 2))
        , _backFaceNormal(normalize(c.backFaceNormal))
        , _backFaceOrigin(_tubeAxisOrigin - _tubeAxisDir * (c.length / 2))
        , _cutAngleRadians(clamp<float>(c.cutAngleRadians, 0, 2 * pi<float>()))
        , _material(c.material)
    {
        _cosCutAngle = cos(_cutAngleRadians);
        _hasInnerCylinderOffset = length2(_innerRadiusOffset) > 0;
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
            float d2 = _distanceToOuterCylinderAxis2(c);
            closestDist2 = min(d2, closestDist2);

            if (_hasInnerCylinderOffset) {
                d2 = _distanceToInnerCylinderAxis2(c);
            }
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

    float valueAt(const vec3& p, float fuzziness, mc::MaterialState& material) const override
    {
        material = _material;

        // A point is inside the ring volume if:
        // - On negative side front and back planes
        // - Between inner and outer radius from axis

        float frontFaceDist = dot(_frontFaceNormal, p - _frontFaceOrigin);
        float backFaceDist = dot(_backFaceNormal, p - _backFaceOrigin);

        // early exit
        if (frontFaceDist > 0 || backFaceDist > 0) {
            return 0;
        }

        vec3 closestPointOnAxis;
        float distanceToOuterCylinderAxis2 = _distanceToOuterCylinderAxis2(p, closestPointOnAxis);
        float distanceToInnerCylinderAxis2 = _hasInnerCylinderOffset
            ? _distanceToInnerCylinderAxis2(p)
            : distanceToOuterCylinderAxis2;

        // early exit
        if (distanceToOuterCylinderAxis2 > _outerRadius2 || distanceToInnerCylinderAxis2 < _innerRadius2) {
            return 0;
        }

        float frontFaceContribution = min<float>(-frontFaceDist / fuzziness, 1);
        float backFaceContribution = min<float>(-backFaceDist / fuzziness, 1);

        float tubeContribution = 1;
        float outerRadiusInner = _outerRadius - fuzziness;
        float innerRadiusInner = _innerRadius + fuzziness;
        float outerRadiusInner2 = outerRadiusInner * outerRadiusInner;
        float innerRadiusInner2 = innerRadiusInner * innerRadiusInner;

        if (distanceToInnerCylinderAxis2 < innerRadiusInner2) {
            // gradient on inner
            float radialDist = sqrt(distanceToInnerCylinderAxis2);
            tubeContribution = (radialDist - _innerRadius) / fuzziness;
        } else if (distanceToOuterCylinderAxis2 > outerRadiusInner2) {
            // gradient on outer
            float radialDist = sqrt(distanceToOuterCylinderAxis2);
            tubeContribution = 1 - ((radialDist - outerRadiusInner) / fuzziness);
        }

        float totalContribution = frontFaceContribution * backFaceContribution * tubeContribution;

        if (_cutAngleRadians > 0) {
            vec3 dir = normalize(p - closestPointOnAxis);
            auto d = dot(_tubeAxisPerp, dir);
            if (d > _cosCutAngle) {
                totalContribution = 0;
            }
        }

        return totalContribution;
    }

private:
    //http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
    // vec3 a = _tubeAxisOrigin;
    // vec3 b = _tubeAxisOrigin + _tubeAxisDir;
    // float d = dot((a - p), (b - a));
    // float t = -d / length2(b - a);
    // vec3 tp = mix(a,b,t);
    // return distance(tp, p);

    inline float _distanceToOuterCylinderAxis2(vec3 p) const
    {
        float t = -dot(_tubeAxisOrigin - p, _tubeAxisDir);
        auto tp = _tubeAxisOrigin + t * _tubeAxisDir;
        return distance2(tp, p);
    }

    inline float _distanceToOuterCylinderAxis2(vec3 p, vec3& pointOnAxis) const
    {
        float t = -dot(_tubeAxisOrigin - p, _tubeAxisDir);
        pointOnAxis = _tubeAxisOrigin + t * _tubeAxisDir;
        return distance2(pointOnAxis, p);
    }

    inline float _distanceToInnerCylinderAxis2(vec3 p) const
    {
        const vec3 origin = _tubeAxisOrigin + _innerRadiusOffset;
        float t = -dot(origin - p, _tubeAxisDir);
        auto tp = origin + t * _tubeAxisDir;
        return distance2(tp, p);
    }

    inline float _distanceToInnerCylinderAxis2(vec3 p, vec3& pointOnAxis) const
    {
        const vec3 origin = _tubeAxisOrigin + _innerRadiusOffset;
        float t = -dot(origin - p, _tubeAxisDir);
        pointOnAxis = origin + t * _tubeAxisDir;
        return distance2(pointOnAxis, p);
    }
};

#endif