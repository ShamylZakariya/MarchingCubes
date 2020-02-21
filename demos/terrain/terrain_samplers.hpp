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

typedef std::function<float(float, float)> NoiseFunction;

class GroundSampler : public mc::IVolumeSampler {
public:

    GroundSampler(NoiseFunction noise, float height)
        :IVolumeSampler(IVolumeSampler::Mode::Additive)
        ,_noise(noise)
        ,_height(height)
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
        auto y = _noise(p.x, p.z) * _height;
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
        // return a wildly wrong normal and shader will use triangle normals
        // return vec3{0,-1,0};

        // TODO: this doesn't work, we need to compute a real normal anyway
        const float d = 1.0f;
        vec3 x0(p + vec3(d, 0, 0));
        vec3 x1(p + vec3(-d, 0, 0));
        vec3 z0(p + vec3(0, 0, d));
        vec3 z1(p + vec3(0, 0, -d));
        x0.y = valueAt(x0);
        x1.y = valueAt(x1);
        z0.y = valueAt(z0);
        z1.y = valueAt(z1);

        return -normalize(cross(x1 - x0, z1 - z0));
    }

private:

    float valueAt(const vec3 &p) const {
        return _noise(p.x, p.z) * _height;
    }

    NoiseFunction _noise;
    float _height;

};

#endif