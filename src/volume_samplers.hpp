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
        , _radius2(radius * radius)
    {
    }

    ~SphereVolumeSampler() override = default;

    bool test(const AABB bounds) const override
    {
        // find the point on surface of bounds closest to _position
        const auto closestPoint = vec3 {
            max(bounds.min.x, min(_position.x, bounds.max.x)),
            max(bounds.min.y, min(_position.y, bounds.max.y)),
            max(bounds.min.z, min(_position.z, bounds.max.z))
        };

        return distance2(_position, closestPoint) <= _radius2;
    }

    float valueAt(const vec3& p, float fuzziness) const override
    {
        float d2 = distance2(p, _position);
        float innerRadius = _radius - fuzziness;
        float min2 = innerRadius * innerRadius;
        if (d2 <= min2) {
            return 1;
        }

        float max2 = _radius * _radius;
        if (d2 >= max2) {
            return 0;
        }

        float d = sqrt(d2) - innerRadius;
        return 1 - (d / fuzziness);
    }

    void setPosition(const vec3& center)
    {
        _position = center;
    }

    void setRadius(float radius)
    {
        _radius = max<float>(radius, 0);
        _radius2 = _radius * _radius;
    }

    vec3 position() const { return _position; }

    float radius() const { return _radius; }

private:
    vec3 _position;
    float _radius;
    float _radius2;
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

    bool test(const AABB bounds) const override
    {
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
        for (const auto& v : bounds.corners()) {
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
        float outerDist = _thickness * 0.5F;
        float innerDist = outerDist - fuzziness;

        if (dist <= innerDist) {
            return 1;
        } else if (dist >= outerDist) {
            return 0;
        }

        return 1 - ((dist - innerDist) / fuzziness);
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

class CubeVolumeSampler : public IVolumeSampler {
public:
    CubeVolumeSampler(vec3 origin, vec3 halfExtents, mat3 rotation, Mode mode)
        : IVolumeSampler(mode)
        , _origin(origin)
        , _halfExtents(max(halfExtents, vec3(0)))
        , _rotation(rotation)
    {
        updateBasis();
    }

    bool test(const AABB bounds) const override
    {
        // find the point on surface of bounds closest to _position
        const auto closestPoint = vec3 {
            max(bounds.min.x, min(_origin.x, bounds.max.x)),
            max(bounds.min.y, min(_origin.y, bounds.max.y)),
            max(bounds.min.z, min(_origin.z, bounds.max.z))
        };

        const auto dir = closestPoint - _origin;
        const auto posXDistance = dot(_posX, dir);
        const auto posYDistance = dot(_posY, dir);
        const auto posZDistance = dot(_posZ, dir);

        const auto posX = posXDistance - _halfExtents.x;
        const auto negX = -posXDistance - _halfExtents.x;
        const auto posY = posYDistance - _halfExtents.y;
        const auto negY = -posYDistance - _halfExtents.y;
        const auto posZ = posZDistance - _halfExtents.z;
        const auto negZ = -posZDistance - _halfExtents.z;

        return (posX <= 0 && negX <= 0 && posY <= 0 && negY <= 0 && posZ <= 0 && negZ <= 0);
    }

    float valueAt(const vec3& p, float fuzziness) const override
    {
        fuzziness += 1e-5F;

        // postivie values denote that the point is on the positive
        // side of the plane, or, outside the cube. if all values
        // are negative, the point is *inside* the cube.

        const auto dir = p - _origin;
        const auto posXDistance = dot(_posX, dir);
        const auto posYDistance = dot(_posY, dir);
        const auto posZDistance = dot(_posZ, dir);

        const auto posX = posXDistance - _halfExtents.x;
        const auto negX = -posXDistance - _halfExtents.x;
        const auto posY = posYDistance - _halfExtents.y;
        const auto negY = -posYDistance - _halfExtents.y;
        const auto posZ = posZDistance - _halfExtents.z;
        const auto negZ = -posZDistance - _halfExtents.z;

        if (posX <= 0 && negX <= 0 && posY <= 0 && negY <= 0 && posZ <= 0 && negZ <= 0) {
            auto posXAmt = -posX / fuzziness;
            auto negXAmt = -negX / fuzziness;
            auto posYAmt = -posY / fuzziness;
            auto negYAmt = -negY / fuzziness;
            auto posZAmt = -posZ / fuzziness;
            auto negZAmt = -negZ / fuzziness;
            return min(min(posXAmt, min(negXAmt, min(posYAmt, min(negYAmt, min(posZAmt, negZAmt))))), 1.0F);
        }
        return 0;
    }

    void setPosition(const vec3& position)
    {
        _origin = position;
        updateBasis();
    }

    vec3 position() const { return _origin; }

    void setSize(const vec3& halfExtents)
    {
        _halfExtents = max(halfExtents, vec3(0));
        updateBasis();
    }

    vec3 size() const { return _halfExtents; }

    void setRotation(const mat3& rotation)
    {
        _rotation = rotation;
        updateBasis();
    }

    mat3 rotation() const { return _rotation; }

    void set(const vec3& position, const vec3& halfExtents, const mat3& rotation)
    {
        _origin = position;
        _halfExtents = max(halfExtents, vec3(0));
        _rotation = rotation;
        updateBasis();
    }

private:
    void updateBasis()
    {
        _basis = glm::translate(mat4 { 1 }, _origin) * mat4(_rotation);
        _posX = vec3(vec4(+1, 0, 0, 1) * _basis);
        _posY = vec3(vec4(0, +1, 0, 1) * _basis);
        _posZ = vec3(vec4(0, 0, +1, 1) * _basis);
    }

private:
    vec3 _origin;
    vec3 _halfExtents;
    mat3 _rotation;
    mat4 _basis;
    vec3 _posX, _posY, _posZ;
};

#endif /* volume_samplers_h */
