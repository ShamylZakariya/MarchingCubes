//
//  volume_samplers.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/27/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef volume_samplers_h
#define volume_samplers_h

#include "util/util.hpp"
#include "volume.hpp"

namespace mc {

namespace volume_samplers_detail {

    /*
    Test for intersection type of the volume defined by vertices and the planar volume defined
    by the plane (origin,normal) with thickness 2*halfExtent. Note, it's assumed
    that the volume defined by vertices is a rectangular prism. It may not be
    strictly necessary, but it's only been tested as such.
    */
    IVolumeSampler::AABBIntersection
    boundedPlaneIntersection(
        glm::vec3 origin,
        glm::vec3 normal,
        float halfExtent,
        const std::array<glm::vec3, 8>& vertices)
    {
        // we need to see if the bounds actually intersects
        int onPositiveSide = 0;
        int onNegativeSide = 0;
        int inside = 0;
        for (const auto& v : vertices) {
            float signedDistance = dot(normal, v - origin);

            if (signedDistance > halfExtent) {
                onPositiveSide++;
            } else if (signedDistance < -halfExtent) {
                onNegativeSide++;
            } else {
                inside++;
            }

            // if we have vertices on both sides of the plane
            // we have an intersection (but not containment)
            if (onPositiveSide && onNegativeSide) {
                return IVolumeSampler::AABBIntersection::IntersectsAABB;
            }
        }

        switch (inside) {
        case 0:
            return IVolumeSampler::AABBIntersection::None;
        case 8:
            return IVolumeSampler::AABBIntersection::ContainsAABB;
        default:
            return IVolumeSampler::AABBIntersection::IntersectsAABB;
        }
    }

    IVolumeSampler::AABBIntersection
    boundedPlaneIntersection(
        glm::vec3 origin,
        glm::vec3 normal,
        float halfExtent,
        util::AABB bounds)
    {
        return boundedPlaneIntersection(origin, normal, halfExtent, bounds.corners());
    }

} // namespace volume_samplers_detail

/*
 Represents a simple sphere
 */
class SphereVolumeSampler : public IVolumeSampler {
public:
    SphereVolumeSampler(glm::vec3 position, float radius, Mode mode)
        : IVolumeSampler(mode)
        , _position(position)
        , _radius(radius)
        , _radius2(radius * radius)
    {
    }

    ~SphereVolumeSampler() override = default;

    bool intersects(util::AABB bounds) const override
    {
        using glm::max;
        using glm::min;

        // early exit if this sphere is contained by bounds
        if (bounds.contains(_position))
            return true;

        // find the point on surface of bounds closest to _position
        const auto closestPoint = glm::vec3 {
            max(bounds.min.x, min(_position.x, bounds.max.x)),
            max(bounds.min.y, min(_position.y, bounds.max.y)),
            max(bounds.min.z, min(_position.z, bounds.max.z))
        };

        // confirm the closest point on the aabb is inside the sphere
        return glm::distance2(_position, closestPoint) <= _radius2;
    }

    AABBIntersection intersection(util::AABB bounds) const override
    {
        int inside = 0;
        for (auto& v : bounds.corners()) {
            if (distance2(v, _position) < _radius2) {
                inside++;
            }
        }

        switch (inside) {
        case 0:
            return IVolumeSampler::AABBIntersection::None;
        case 8:
            return IVolumeSampler::AABBIntersection::ContainsAABB;
        default:
            return IVolumeSampler::AABBIntersection::IntersectsAABB;
        }
    }

    float valueAt(const glm::vec3& p, float fuzziness) const override
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

    void setPosition(const glm::vec3& center)
    {
        _position = center;
    }

    void setRadius(float radius)
    {
        _radius = glm::max<float>(radius, 0);
        _radius2 = _radius * _radius;
    }

    glm::vec3 position() const { return _position; }

    float radius() const { return _radius; }

private:
    glm::vec3 _position;
    float _radius;
    float _radius2;
};

/*
HalfspaceVolumeSampler represents a plane, where values on the positive side
(e.g., in the direction of the normal) are NOT in the plane's volume, and values
on the negative side are inside.
*/
class HalfspaceVolumeSampler : public IVolumeSampler {
public:
    HalfspaceVolumeSampler(glm::vec3 planeOrigin, glm::vec3 planeNormal, Mode mode)
        : IVolumeSampler(mode)
        , _origin(planeOrigin)
        , _normal(planeNormal)
    {
    }

    bool intersects(util::AABB bounds) const override
    {
        for (const auto& v : bounds.corners()) {
            auto signedDistance = dot(_normal, v - _origin);
            if (signedDistance < 0) {
                return true;
            }
        }
        return false;
    }

    AABBIntersection intersection(util::AABB bounds) const override
    {
        int inside = 0;
        for (const auto& v : bounds.corners()) {
            auto signedDistance = dot(_normal, v - _origin);
            if (signedDistance < 0) {
                inside++;
            }
        }

        switch (inside) {
        case 0:
            return IVolumeSampler::AABBIntersection::None;
        case 8:
            return IVolumeSampler::AABBIntersection::ContainsAABB;
        default:
            return IVolumeSampler::AABBIntersection::IntersectsAABB;
        }
    }

    float valueAt(const glm::vec3& p, float fuzziness) const override
    {
        float signedDist = dot(_normal, p - _origin);
        if (signedDist < -fuzziness) {
            return 1;
        } else if (signedDist > 0) {
            return 0;
        }
        return -signedDist / fuzziness;
    }

    void setPlaneOrigin(const glm::vec3 planeOrigin) { _origin = planeOrigin; }
    glm::vec3 planeOrigin() const { return _origin; }

    void setPlaneNormal(const glm::vec3 planeNormal) { _normal = glm::normalize(planeNormal); }
    glm::vec3 planeNormal() const { return _normal; }

private:
    glm::vec3 _origin;
    glm::vec3 _normal;
};

/*
 BoundedPlaneVolumeSampler represents a plane with a thickness; points
 in space less than that thickness from the plane are considered "inside"
 the plane.
 */
class BoundedPlaneVolumeSampler : public IVolumeSampler {
public:
    BoundedPlaneVolumeSampler(glm::vec3 planeOrigin, glm::vec3 planeNormal, float planeThickness, Mode mode)
        : IVolumeSampler(mode)
        , _origin(planeOrigin)
        , _normal(glm::normalize(planeNormal))
        , _thickness(std::max<float>(planeThickness, 0))
    {
    }

    bool intersects(util::AABB bounds) const override
    {
        return volume_samplers_detail::boundedPlaneIntersection(_origin, _normal, _thickness / 2, bounds) != AABBIntersection::None;
    }

    AABBIntersection intersection(util::AABB bounds) const override
    {
        return volume_samplers_detail::boundedPlaneIntersection(_origin, _normal, _thickness / 2, bounds);
    }

    float valueAt(const glm::vec3& p, float fuzziness) const override
    {
        // distance of p from plane
        float dist = abs(glm::dot(_normal, p - _origin));
        float outerDist = _thickness * 0.5F;
        float innerDist = outerDist - fuzziness;

        if (dist <= innerDist) {
            return 1;
        } else if (dist >= outerDist) {
            return 0;
        }

        return 1 - ((dist - innerDist) / fuzziness);
    }

    void setPlaneOrigin(const glm::vec3 planeOrigin) { _origin = planeOrigin; }
    glm::vec3 planeOrigin() const { return _origin; }

    void setPlaneNormal(const glm::vec3 planeNormal) { _normal = glm::normalize(planeNormal); }
    glm::vec3 planeNormal() const { return _normal; }

    void setThickness(float thickness) { _thickness = std::max<float>(thickness, 0); }
    float thickness() const { return _thickness; }

private:
    glm::vec3 _origin;
    glm::vec3 _normal;
    float _thickness;
};

class RectangularPrismVolumeSampler : public IVolumeSampler {
public:
    RectangularPrismVolumeSampler(glm::vec3 origin, glm::vec3 halfExtents, glm::mat3 rotation, Mode mode)
        : IVolumeSampler(mode)
        , _origin(origin)
        , _halfExtents(max(halfExtents, glm::vec3(0)))
        , _rotation(rotation)
    {
        _update();
    }

    bool intersects(util::AABB bounds) const override
    {
        using volume_samplers_detail::boundedPlaneIntersection;

        // coarse AABB check
        if (bounds.intersect(_bounds) == util::AABB::Intersection::Outside) {
            return false;
        }

        // now check if the aabb intersects the bounded planar volumes
        // represented along x, y and z. This is like a simplification
        // of separating axes theorem when one prism is an AABB.
        // if all three volumes intersect bounds, we know bounds intersects
        // our prism
        std::array<glm::vec3, 8> corners;
        bounds.corners(corners);
        if (boundedPlaneIntersection(_origin, _posX, _halfExtents.x, corners) == AABBIntersection::None) {
            return false;
        }
        if (boundedPlaneIntersection(_origin, _posY, _halfExtents.y, corners) == AABBIntersection::None) {
            return false;
        }
        if (boundedPlaneIntersection(_origin, _posZ, _halfExtents.z, corners) == AABBIntersection::None) {
            return false;
        }

        return true;
    }

    AABBIntersection intersection(util::AABB bounds) const override
    {
        using volume_samplers_detail::boundedPlaneIntersection;

        // coarse AABB check
        if (bounds.intersect(_bounds) == util::AABB::Intersection::Outside) {
            return AABBIntersection::None;
        }

        std::array<glm::vec3, 8> corners;
        bounds.corners(corners);

        // check for intersection type; if it's not ContainsAABB, early exit
        // if all 3 are ContainsAABB, we can return ContainsAABB
        auto intersection = boundedPlaneIntersection(_origin, _posX, _halfExtents.x, corners);
        if (intersection != AABBIntersection::ContainsAABB) {
            return intersection;
        }

        intersection = boundedPlaneIntersection(_origin, _posY, _halfExtents.y, corners);
        if (intersection != AABBIntersection::ContainsAABB) {
            return intersection;
        }

        intersection = boundedPlaneIntersection(_origin, _posZ, _halfExtents.z, corners);
        if (intersection != AABBIntersection::ContainsAABB) {
            return intersection;
        }

        return AABBIntersection::ContainsAABB;
    }

    float valueAt(const glm::vec3& p, float fuzziness) const override
    {
        using glm::min;

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

    void setPosition(const glm::vec3& position)
    {
        _origin = position;
        _update();
    }

    glm::vec3 position() const { return _origin; }

    void setHalfExtents(const glm::vec3& halfExtents)
    {
        _halfExtents = max(halfExtents, glm::vec3(0));
        _update();
    }

    glm::vec3 halfExtents() const { return _halfExtents; }

    void setRotation(const glm::mat3& rotation)
    {
        _rotation = glm::mat4 { rotation };
        _update();
    }

    glm::mat3 rotation() const { return _rotation; }

    void set(const glm::vec3& position, const glm::vec3& halfExtents, const glm::mat3& rotation)
    {
        _origin = position;
        _halfExtents = max(halfExtents, glm::vec3(0));
        _rotation = rotation;
        _update();
    }

    util::AABB bounds() const { return _bounds; }

    std::array<glm::vec3, 8> corners() const
    {
        return _corners;
    }

    void addTo(util::LineSegmentBuffer& lineBuffer, const glm::vec4& color) const
    {
        auto corners = this->corners();

        // trace bottom
        {
            using V = util::LineSegmentBuffer::vertex_type;
            lineBuffer.add(V { corners[0], color }, V { corners[1], color });
            lineBuffer.add(V { corners[1], color }, V { corners[2], color });
            lineBuffer.add(V { corners[2], color }, V { corners[3], color });
            lineBuffer.add(V { corners[3], color }, V { corners[0], color });

            // trace top
            lineBuffer.add(V { corners[4], color }, V { corners[5], color });
            lineBuffer.add(V { corners[5], color }, V { corners[6], color });
            lineBuffer.add(V { corners[6], color }, V { corners[7], color });
            lineBuffer.add(V { corners[7], color }, V { corners[4], color });

            // add bars connecting bottom to top
            lineBuffer.add(V { corners[0], color }, V { corners[4], color });
            lineBuffer.add(V { corners[1], color }, V { corners[5], color });
            lineBuffer.add(V { corners[2], color }, V { corners[6], color });
            lineBuffer.add(V { corners[3], color }, V { corners[7], color });
        }
    }

private:
    void _update()
    {
        // extract plane normals from rotation
        _posX = glm::vec3 { _rotation[0][0], _rotation[1][0], _rotation[2][0] };
        _posY = glm::vec3 { _rotation[0][1], _rotation[1][1], _rotation[2][1] };
        _posZ = glm::vec3 { _rotation[0][2], _rotation[1][2], _rotation[2][2] };

        // generate _corners
        auto e = _halfExtents;
        _corners[0] = _origin + (glm::vec3(+e.x, -e.y, -e.z) * _rotation);
        _corners[1] = _origin + (glm::vec3(+e.x, -e.y, +e.z) * _rotation);
        _corners[2] = _origin + (glm::vec3(-e.x, -e.y, +e.z) * _rotation);
        _corners[3] = _origin + (glm::vec3(-e.x, -e.y, -e.z) * _rotation);
        _corners[4] = _origin + (glm::vec3(+e.x, +e.y, -e.z) * _rotation);
        _corners[5] = _origin + (glm::vec3(+e.x, +e.y, +e.z) * _rotation);
        _corners[6] = _origin + (glm::vec3(-e.x, +e.y, +e.z) * _rotation);
        _corners[7] = _origin + (glm::vec3(-e.x, +e.y, -e.z) * _rotation);

        _bounds.invalidate();
        for (auto& c : _corners) {
            _bounds.add(c);
        }
    }

private:
    glm::vec3 _origin;
    glm::vec3 _halfExtents;
    glm::mat3 _rotation;
    glm::vec3 _posX, _posY, _posZ;

    std::array<glm::vec3, 8> _corners;
    util::AABB _bounds;
};

} // namespace mc

#endif /* volume_samplers_h */
