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

/*
 Represents a simple sphere
 */
class SphereVolumeSampler : public IVolumeSampler {
public:
    SphereVolumeSampler(vec3 position, float radius, Mode mode)
        : IVolumeSampler(mode)
        , _position(position)
        , _radius(radius)
    {
        updateBounds();
    }

    ~SphereVolumeSampler() override = default;

    bool test(const AABB bounds) const override {
        return _bounds.intersect(bounds) != AABB::Intersection::Outside;
    }
    
    float valueAt(const vec3& p, float fuzziness) const override
    {
        float d2 = distance2(p, _position);
        float min2 = (_radius - fuzziness) * (_radius - fuzziness);
        if (d2 < min2) {
            return 1;
        }

        float max2 = _radius * _radius;
        if (d2 > max2) {
            return 0;
        }

        float d = sqrt(d2) - _radius;
        return 1 - (d / fuzziness);
    }

    void setPosition(const vec3& center) { _position = center; updateBounds(); }
    void setRadius(float radius) { _radius = max<float>(radius, 0); updateBounds(); }
    vec3 position() const { return _position; }
    float radius() const { return _radius; }

private:
    
    void updateBounds() {
        _bounds = AABB(_position, _radius);
    }
    
private:
    
    vec3 _position;
    float _radius;
    AABB _bounds;
};

/*
 BoundedPlaneVolumeSampler represents a plane with a thickness; points
 in space less than that thickness from the plane are considered "inside"
 the plane.
 */
class BoundedPlaneVolumeSampler : public IVolumeSampler {
public:
    BoundedPlaneVolumeSampler(vec3 planeOrigin, vec3 planeNormal, float planeThickness, Mode mode)
        : IVolumeSampler(mode)
        , _origin(planeOrigin)
        , _normal(::normalize(planeNormal))
        , _thickness(std::max<float>(planeThickness, 0))
    {
    }

    bool test(const AABB bounds) const override {
        float halfThickness = _thickness * 0.5F;
        
        // quick test for early exit
        float dist = abs(dot(_normal, bounds.center() - _origin)) - halfThickness;
        float dist2 = dist * dist;
        if (dist2 > bounds.radius2()) {
            return false;
        }
        
        // we need to see if the bounds actually intersects
        int onPositiveSide = 0;
        int onNegativeSide = 0;
        for (const auto &v : bounds.vertices()) {
            float signedDistance = dot(_normal, v - _origin);

            if (abs(signedDistance) < halfThickness) {
                // if any vertices are inside the plane thickness we
                // have an intersection
                return true;
            } else if (signedDistance > 0) {
                onPositiveSide++;
            } else {
                onNegativeSide++;
            }
            
            // if we have vertices on both sides of the plane
            // we have an intersection
            if (onPositiveSide && onNegativeSide) {
                return true;
            }
        }
        
        return false;
    }

    float valueAt(const vec3& p, float fuzziness) const override
    {
        // distance of p from plane
        float dist = abs(dot(_normal, p - _origin));
        float halfThickness = _thickness * 0.5F;

        if (dist <= halfThickness - fuzziness) {
            return 1;
        } else if (dist > halfThickness) {
            return 0;
        }
        dist -= halfThickness;
        return 1 - (dist / fuzziness);
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
