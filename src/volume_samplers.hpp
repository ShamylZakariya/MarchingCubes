//
//  volume_samplers.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/27/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef volume_samplers_h
#define volume_samplers_h

#include <glm/gtx/intersect.hpp>

#include "volume.hpp"

class SphereVolumeSampler : public IVolumeSampler {
public:
    SphereVolumeSampler(vec3 position, float radius, Mode mode)
        : IVolumeSampler(mode)
        , _position(position)
        , _radius(radius)
    {
    }

    ~SphereVolumeSampler() override = default;

    AABB bounds() const override
    {
        return AABB(_position, _radius);
    }

    float valueAt(const vec3& p, float falloffThreshold) const override
    {
        float d2 = distance2(p, _position);
        float min2 = _radius * _radius;
        if (d2 < min2) {
            return 1;
        }

        float max2 = (_radius + falloffThreshold) * (_radius + falloffThreshold);
        if (d2 > max2) {
            return 0;
        }

        float d = sqrt(d2) - _radius;
        return 1 - (d / falloffThreshold);
    }

    void setPosition(const vec3& center) { _position = center; }
    void setRadius(float radius) { _radius = max<float>(radius, 0); }
    vec3 position() const { return _position; }
    float radius() const { return _radius; }

private:
    vec3 _position;
    float _radius;
};

class PlaneVolumeSampler : public IVolumeSampler {
public:
    PlaneVolumeSampler(vec3 planeOrigin, vec3 planeNormal, float planeThickness, Mode mode)
        : IVolumeSampler(mode)
        , _origin(planeOrigin)
        , _normal(::normalize(planeNormal))
        , _thickness(std::max<float>(planeThickness, 0))
    {
    }

    // a plane has no meaningful bounds
    AABB bounds() const override { return AABB(); }

    float valueAt(const vec3& p, float falloffThreshold) const override
    {
        // distance of p from plane
        float dist = abs(dot(_normal, p - _origin));

        if (dist <= _thickness) {
            return 1;
        } else if (dist > _thickness + falloffThreshold) {
            return 0;
        }
        dist -= _thickness;
        return 1 - (dist / falloffThreshold);
    }

    void setPlaneOrigin(const vec3 planeOrigin) { _origin = planeOrigin; }
    vec3 planeOrigin() const { return _origin; }

    void setPlaneNormal(const vec3 planeNormal) { _normal = ::normalize(planeNormal); }
    vec3 planeNormal() const { return _normal; }

    void setThickness(float thickness) { _thickness = std::max<float>(thickness, 0); }
    float thickness() const { return _thickness; }

private:
    vec3 _origin;
    vec3 _normal;
    float _thickness;
};

#endif /* volume_samplers_h */
