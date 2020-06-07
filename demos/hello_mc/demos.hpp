#ifndef demos_hpp
#define demos_hpp

#include <memory>

#include <epoxy/gl.h>

#include <mc/util/unowned_ptr.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;

//
//  Geometry Demos
//

const mc::MaterialState kDefaultMaterial {
    glm::vec4(1, 1, 1, 1),
    0,
    0,
    0
};


class Demo {
public:
    Demo() = default;
    virtual ~Demo() = default;

    virtual void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) = 0;
    virtual void step(float time) { }
    virtual void drawDebugLines(mc::util::LineSegmentBuffer&) { }
};

class CubeDemo : public Demo {
public:
    CubeDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;

        _rect = volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(10, 10, 10), mat3 { 1 }, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));
    }

    void step(float time) override
    {
        float angle = static_cast<float>(M_PI * time * 0.1);
        _rect->setRotation(rotate(mat4 { 1 }, angle, normalize(vec3(0, 1, 1))));
    }

    void drawDebugLines(mc::util::LineSegmentBuffer& debugLinebuf) override
    {
        debugLinebuf.add(_rect->bounds(), vec4(1, 1, 0, 1));
        _rect->addTo(debugLinebuf, vec4(0, 1, 1, 1));
    }

private:
    mc::util::unowned_ptr<mc::RectangularPrismVolumeSampler> _rect;
};

class SphereDemo : public Demo {
public:
    SphereDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        _pos = size / 2.0F;
        _radius = 10;

        _sphere = volume->add(std::make_unique<mc::SphereVolumeSampler>(
            _pos, _radius, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));
    }

    void step(float time) override
    {
        auto cycle = static_cast<float>(M_PI * time * 0.1);
        auto yOffset = sin(cycle) * 5;
        auto radiusOffset = cos(cycle) * _radius * 0.25F;

        _sphere->setPosition(_pos + vec3(0, yOffset, 0));
        _sphere->setRadius(_radius + radiusOffset);
    }

private:
    vec3 _pos;
    float _radius;
    mc::util::unowned_ptr<mc::SphereVolumeSampler> _sphere;
};

class BoundedPlaneDemo : public Demo {
public:
    BoundedPlaneDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;
        _plane = volume->add(std::make_unique<mc::BoundedPlaneVolumeSampler>(
            center, planeNormal(mat4 { 1 }), 10, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));
    }

    void step(float time) override
    {
        auto angle = static_cast<float>(pi<float>() * -time * 0.2F);
        auto rot = rotate(mat4 { 1 }, angle, normalize(vec3(1, 1, 0)));
        _plane->setPlaneNormal(planeNormal(rot));
    }

private:
    static vec3 planeNormal(const glm::mat4& rot)
    {
        return vec3(rot[0][1], rot[1][1], rot[2][1]);
    }

    mc::util::unowned_ptr<mc::BoundedPlaneVolumeSampler> _plane;
    mat3 _rotation { 1 };
};

class HalfspaceDemo : public Demo {
public:
    HalfspaceDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;
        _plane = volume->add(std::make_unique<mc::HalfspaceVolumeSampler>(
            center, planeNormal(mat4 { 1 }), kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));
    }

    void step(float time) override
    {
        auto angle = static_cast<float>(pi<float>() * -time * 0.2F);
        auto rot = rotate(mat4 { 1 }, angle, normalize(vec3(1, 1, 0)));
        _plane->setPlaneNormal(planeNormal(rot));
    }

private:
    static vec3 planeNormal(const glm::mat4& rot)
    {
        return vec3(rot[0][1], rot[1][1], rot[2][1]);
    }

    mc::util::unowned_ptr<mc::HalfspaceVolumeSampler> _plane;
    mat3 _rotation { 1 };
};

class CompoundShapesDemo : public Demo {
public:
    CompoundShapesDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = volume->size();
        std::random_device rng;
        std::default_random_engine gen { static_cast<long unsigned int>(12345) };

        _bottomPlane = volume->add(std::make_unique<mc::HalfspaceVolumeSampler>(
            vec3(0, size.y * 0.35, 0), vec3(0, 1, 0), kDefaultMaterial, mc::IVolumeSampler::Mode::Subtractive));

        const int numSpheres = 30;
        const int numCubes = 10;

        if (numSpheres > 0) {
            // create spheres
            auto maxRadius = size.x / 6;
            std::uniform_real_distribution<float> xDist(maxRadius, size.x - maxRadius);
            std::uniform_real_distribution<float> zDist(maxRadius, size.z - maxRadius);
            std::uniform_real_distribution<float> yDist(size.y * 0.4, size.y * 0.6);
            std::uniform_real_distribution<float> radiusDist(size.x / 20, maxRadius);
            std::uniform_real_distribution<float> bobRateDist(0.4, 2);
            std::uniform_real_distribution<float> bobPhaseDist(0, pi<float>());
            std::uniform_real_distribution<float> bobExtentDist(size.y * 0.0625, size.y * 0.125);

            for (int i = 0; i < 40; i++) {
                auto pos = vec3 { xDist(gen), yDist(gen), zDist(gen) };
                auto radius = radiusDist(gen);
                auto rate = bobRateDist(gen);
                auto phase = bobPhaseDist(gen);
                auto bobExtent = bobExtentDist(gen);

                auto sphere = volume->add(std::make_unique<mc::SphereVolumeSampler>(
                    pos, radius, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));

                _spheres.push_back(SphereState {
                    sphere, pos, rate, phase, bobExtent });
            }
        }

        if (numCubes > 0) {
            auto maxSize = size.x / 5;
            std::uniform_real_distribution<float> xDist(maxSize, size.x - maxSize);
            std::uniform_real_distribution<float> zDist(maxSize, size.z - maxSize);
            std::uniform_real_distribution<float> yDist(size.y * 0.4, size.y * 0.6);
            std::uniform_real_distribution<float> sizeDist(size.x / 10, maxSize);
            std::uniform_real_distribution<float> bobRateDist(0.4, 2);
            std::uniform_real_distribution<float> bobPhaseDist(0, pi<float>());
            std::uniform_real_distribution<float> bobExtentDist(size.y * 0.0625, size.y * 0.125);
            std::uniform_real_distribution<float> spinRateDist(0.2, 0.6);
            std::uniform_real_distribution<float> spinPhaseDist(0, pi<float>());
            std::uniform_real_distribution<float> axisComponentDist(-1, 1);

            for (int i = 0; i < numCubes; i++) {
                auto pos = vec3 { xDist(gen), yDist(gen), zDist(gen) };
                auto size = sizeDist(gen);
                auto bobRate = bobRateDist(gen);
                auto bobPhase = bobPhaseDist(gen);
                auto bobExtent = bobExtentDist(gen);
                auto spinRate = spinRateDist(gen);
                auto spinPhase = spinPhaseDist(gen);
                auto spinAxis = normalize(vec3 {
                    axisComponentDist(gen),
                    axisComponentDist(gen),
                    axisComponentDist(gen) });

                auto cube = volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
                    pos, vec3(size) / 2.0F, rotation(0, spinPhase, spinRate, spinAxis),
                    kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));

                _cubes.push_back(CubeState {
                    cube,
                    pos,
                    bobRate,
                    bobPhase,
                    bobExtent,
                    spinRate,
                    spinPhase,
                    spinAxis });
            }
        }
    }

    void step(float time) override
    {
        for (auto& sphereState : _spheres) {
            auto cycle = time * sphereState.rate + sphereState.phase;
            vec3 pos = sphereState.position;
            pos.y += sphereState.bobExtent * sin(cycle);
            sphereState.shape->setPosition(pos);
        }

        for (auto& cubeState : _cubes) {
            auto bobExtent = cubeState.bobExtent * sin(time * cubeState.bobRate + cubeState.bobPhase);
            auto rot = rotation(time, cubeState.spinPhase, cubeState.spinRate, cubeState.spinAxis);
            auto pos = cubeState.position + vec3(0, bobExtent, 0);
            cubeState.shape->set(pos, cubeState.shape->halfExtents(), rot);
        }
    }

private:
    mat3 rotation(float time, float phase, float rate, vec3 axis)
    {
        auto angle = sin(time * rate + phase);
        return rotate(mat4 { 1 }, angle, axis);
    }

    struct SphereState {
        mc::util::unowned_ptr<mc::SphereVolumeSampler> shape;
        vec3 position;
        float rate;
        float phase;
        float bobExtent;
    };

    struct CubeState {
        mc::util::unowned_ptr<mc::RectangularPrismVolumeSampler> shape;
        vec3 position;
        float bobRate;
        float bobPhase;
        float bobExtent;
        float spinRate;
        float spinPhase;
        vec3 spinAxis;
    };

    std::vector<SphereState> _spheres;
    std::vector<CubeState> _cubes;
    mc::util::unowned_ptr<mc::HalfspaceVolumeSampler> _bottomPlane;
};

class SubtractiveCubeDemo : public Demo {
public:
    SubtractiveCubeDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;

        volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(10, 10, 10), mat3 { 1 }, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));

        _cube = volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center + vec3(0, -5, 0), vec3(10, 10, 10), mat3 { 1 }, kDefaultMaterial, mc::IVolumeSampler::Mode::Subtractive));
    }

    void step(float time) override
    {
        auto angle = static_cast<float>(pi<float>() * -time * 0.2F);
        auto rot = rotate(mat4 { 1 }, angle, normalize(vec3(1, 1, 0)));
        _cube->setRotation(mat3(rot));
    }

private:
    mc::util::unowned_ptr<mc::RectangularPrismVolumeSampler> _cube;
};

class SubtractiveHalfspaceDemo : public Demo {
public:
    SubtractiveHalfspaceDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;

        volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(10, 10, 10), mat3 { 1 }, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));

        _plane = volume->add(std::make_unique<mc::HalfspaceVolumeSampler>(
            center, planeNormal(mat4 { 1 }), kDefaultMaterial, mc::IVolumeSampler::Mode::Subtractive));
    }

    void step(float time) override
    {
        auto angle = static_cast<float>(pi<float>() * -time * 0.2F);
        auto rot = rotate(mat4 { 1 }, angle, normalize(vec3(1, 1, 0)));
        _plane->setPlaneNormal(planeNormal(rot));
    }

private:
    static vec3 planeNormal(const glm::mat4& rot)
    {
        return vec3(rot[0][1], rot[1][1], rot[2][1]);
    }

    mc::util::unowned_ptr<mc::HalfspaceVolumeSampler> _plane;
    mat3 _rotation { 1 };
};

class SubtractiveSphereDemo : public Demo {
public:
    SubtractiveSphereDemo() = default;

    void build(mc::util::unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;

        volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(10, 10, 10), mat3 { 1 }, kDefaultMaterial, mc::IVolumeSampler::Mode::Additive));

        volume->add(std::make_unique<mc::SphereVolumeSampler>(
            center + vec3(0, -10, 0), 10, kDefaultMaterial, mc::IVolumeSampler::Mode::Subtractive));
    }
};

//
//  Demo Registry
//

typedef std::function<std::unique_ptr<Demo>()> DemoFactory;
typedef std::pair<const char*, DemoFactory> DemoEntry;

const static std::vector<DemoEntry> DemoRegistry = {
    DemoEntry("SubtractiveCube", []() { return std::make_unique<SubtractiveCubeDemo>(); }),
    DemoEntry("SubtractiveHalfspace", []() { return std::make_unique<SubtractiveHalfspaceDemo>(); }),
    DemoEntry("SubtractiveSphere", []() { return std::make_unique<SubtractiveSphereDemo>(); }),
    DemoEntry("Cube", []() { return std::make_unique<CubeDemo>(); }),
    DemoEntry("Sphere", []() { return std::make_unique<SphereDemo>(); }),
    DemoEntry("BoundedPlane", []() { return std::make_unique<BoundedPlaneDemo>(); }),
    DemoEntry("Halfspace", []() { return std::make_unique<HalfspaceDemo>(); }),
    DemoEntry("CompoundShapes", []() { return std::make_unique<CompoundShapesDemo>(); }),
};

#endif