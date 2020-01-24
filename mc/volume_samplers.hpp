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

#include "util/util.hpp"
#include "volume.hpp"

namespace mc {

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

    bool intersects(const util::AABB bounds) const override
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

    bool intersects(const util::AABB bounds) const override
    {
        using glm::dot;
        using glm::max;
        using glm::min;

        // find point on plane closest to bounds
        auto bc = bounds.center();
        auto distanceToBoundsCenter = dot(_normal, bc - _origin);
        auto closestPointOnPlane = bc + _normal * -distanceToBoundsCenter;

        // find the point bounds closest to closestPointOnPlane
        const auto closestPointOnBounds = glm::vec3 {
            max(bounds.min.x, min(closestPointOnPlane.x, bounds.max.x)),
            max(bounds.min.y, min(closestPointOnPlane.y, bounds.max.y)),
            max(bounds.min.z, min(closestPointOnPlane.z, bounds.max.z))
        };

        // now test that closestPointOnBounds is inside our volume
        float halfThickness = _thickness * 0.5F;
        auto distance = dot(_normal, (closestPointOnBounds - _origin));
        return (distance < halfThickness && distance > -halfThickness);
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

    bool intersects(const util::AABB bounds) const override
    {
        // this is really coarse, but doesn't cause mismatches
        return (bounds.intersect(_bounds) != util::AABB::Intersection::Outside);
    }

    bool intersects__experimental_bad(const util::AABB bounds) const
    {
        // early exit if this prism is contained by bounds
        if (bounds.contains(_origin))
            return true;

        // coarse AABB check
        if (bounds.intersect(_bounds) != util::AABB::Intersection::Outside) {

            // first, the easy pass - see if any of our vertices are contained by bounds
            for (const auto& c : _corners) {
                if (bounds.contains(c)) {
                    return true;
                }
            }

            // TODO: now we have to go into the separating axis theorem
            // http://www.jkh.me/files/tutorials/Separating%20Axis%20Theorem%20for%20Oriented%20Bounding%20Boxes.pdf
        }
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

    void setSize(const glm::vec3& halfExtents)
    {
        _halfExtents = max(halfExtents, glm::vec3(0));
        _update();
    }

    glm::vec3 size() const { return _halfExtents; }

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
        using util::Vertex;
        auto corners = this->corners();

        // trace bottom
        lineBuffer.add(Vertex { corners[0], color }, Vertex { corners[1], color });
        lineBuffer.add(Vertex { corners[1], color }, Vertex { corners[2], color });
        lineBuffer.add(Vertex { corners[2], color }, Vertex { corners[3], color });
        lineBuffer.add(Vertex { corners[3], color }, Vertex { corners[0], color });

        // trace top
        lineBuffer.add(Vertex { corners[4], color }, Vertex { corners[5], color });
        lineBuffer.add(Vertex { corners[5], color }, Vertex { corners[6], color });
        lineBuffer.add(Vertex { corners[6], color }, Vertex { corners[7], color });
        lineBuffer.add(Vertex { corners[7], color }, Vertex { corners[4], color });

        // add bars connecting bottom to top
        lineBuffer.add(Vertex { corners[0], color }, Vertex { corners[4], color });
        lineBuffer.add(Vertex { corners[1], color }, Vertex { corners[5], color });
        lineBuffer.add(Vertex { corners[2], color }, Vertex { corners[6], color });
        lineBuffer.add(Vertex { corners[3], color }, Vertex { corners[7], color });
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
