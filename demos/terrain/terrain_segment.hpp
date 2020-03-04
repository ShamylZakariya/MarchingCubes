#ifndef terrain_segment_hpp
#define terrain_segment_hpp

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include <mc/marching_cubes.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "FastNoise.h"
#include "materials.hpp"
#include "terrain_samplers.hpp"
#include "xorshift.hpp"

struct TerrainSegment {
public:
    TerrainSegment(int size, std::shared_ptr<mc::util::ThreadPool> threadPool, FastNoise& noise)
        : idx(-1)
        , size(size)
        , threadPool(threadPool)
        , noise(noise)
    {
        std::vector<mc::util::unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
        for (size_t i = 0, N = threadPool->size(); i < N; i++) {
            triangles.push_back(std::make_unique<mc::TriangleConsumer<mc::Vertex>>());
            unownedTriangleConsumers.push_back(triangles.back().get());
        }

        volume = std::make_unique<mc::OctreeVolume>(size, 4, 4, threadPool, unownedTriangleConsumers);
        heightmap.resize((size + 1) * (size + 1));
    }

    ~TerrainSegment() = default;
    TerrainSegment(const TerrainSegment&) = delete;
    TerrainSegment(TerrainSegment&&) = delete;
    TerrainSegment& operator==(const TerrainSegment&) = delete;

    void build(int idx, FastNoise& noise)
    {
        using namespace glm;

        std::cout << "[TerrainSegment::build] idx " << idx << std::endl;

        this->idx = idx;
        volume->clear();
        triangleCount = 0;
        for (auto& tc : triangles) {
            tc->clear();
        }

        waypoints.clear();
        waypointLineBuffer.clear();

        const auto segmentColor = rainbow(static_cast<float>(idx % 5) / 5.0F);

        //
        // build terrain sampler
        //

        const mc::MaterialState floorTerrainMaterial {
            vec4(1),
            1,
            0,
            0
        };

        const mc::MaterialState lowTerrainMaterial {
            vec4(1),
            0,
            1,
            0
        };

        const mc::MaterialState highTerrainMaterial {
            vec4(0.7, 0.7, 0.7, 1),
            0,
            1,
            1
        };

        const mc::MaterialState archMaterial {
            vec4(0.5, 0.5, 0.5, 1),
            0.3,
            0,
            1
        };

        const float floorThreshold = 1.25F;
        const float zOffset = idx * size;

        volume->add(std::make_unique<HeightmapSampler>(heightmap.data(), size, maxTerrainHeight, floorThreshold,
            floorTerrainMaterial, lowTerrainMaterial, highTerrainMaterial));

        //
        //  Build an RNG seeded for this segment
        //

        auto rng = rng_xorshift64 { static_cast<uint64_t>(12345 * (idx + 1)) };

        //
        // build a broken arch
        //

        const auto size = vec3(volume->size());
        const auto center = size / 2.0F;
        const auto maxArches = 7;
        for (int i = 0; i < maxArches; i++) {

            // roll the dice to see if we get an arch here
            if (rng.nextInt(10) < 5) {
                continue;
            }

            // we have an arch, get its z position, use that to feed simplex
            // noise to perturb x position smoothly. Note - we inset a bit to
            // reduce likelyhood of clipping
            // TODO: Use simplex to seed our arches too... this would prevent clipping
            // because arches would clone across segment boundaries
            float archZ = 30 + (this->size - 60) * static_cast<float>(i) / maxArches;
            float archX = center.x + noise.GetSimplex(archZ + zOffset, 0) * size.x * 0.125F;

            Tube::Config arch;
            arch.axisOrigin = vec3 { archX, 0, archZ };

            arch.innerRadiusAxisOffset = vec3(0, rng.nextFloat(4, 10), 0);

            arch.axisDir = normalize(vec3(rng.nextFloat(-0.6, 0.6), rng.nextFloat(-0.2, 0.2), 1));

            arch.axisPerp = normalize(vec3(rng.nextFloat(-0.2, 0.2), 1, 0));
            arch.length = rng.nextFloat(7, 11);
            arch.innerRadius = rng.nextFloat(35, 43);
            arch.outerRadius = rng.nextFloat(48, 55);
            arch.frontFaceNormal = arch.axisDir;
            arch.backFaceNormal = -arch.axisDir;
            arch.cutAngleRadians = radians(rng.nextFloat(16, 32));
            arch.material = archMaterial;

            volume->add(std::make_unique<Tube>(arch));

            vec3 waypoint = arch.axisOrigin + arch.axisPerp * arch.innerRadius * rng.nextFloat(0.2F, 0.8F);
            waypoint.y = std::max(waypoint.y, maxTerrainHeight);
            waypoints.push_back(waypoint);
            waypointLineBuffer.addMarker(waypoint, 4, segmentColor);
        }

        // all segments have to have at least 1 waypoint; if the dice roll
        // didn't give us any arches, make a default waypoint
        if (waypoints.empty()) {
            vec3 waypoint = vec3(center.x, maxTerrainHeight + rng.nextFloat(10), center.z);
            waypoints.push_back(waypoint);
            waypointLineBuffer.addMarker(waypoint, 4, vec4(1, 1, 0, 1));
        }

        //
        //  Build a debug frame to show our volume
        //

        boundingLineBuffer.clear();
        boundingLineBuffer.add(AABB(vec3 { 0.0F }, size).inset(1), segmentColor);
    }

    void march()
    {
        const double startTime = glfwGetTime();

        aabbLineBuffer.clear();

        const auto nodeObserver = [this](mc::OctreeVolume::Node* node) {
            {
                // update the occupied aabb display
                auto bounds = node->bounds;
                bounds.inset(node->depth * 0.005F);
                aabbLineBuffer.add(bounds, nodeColor(node->depth));
            }
        };

        const auto onMarchComplete = [this,startTime]() {
            lastMarchDurationSeconds = glfwGetTime() - startTime;

            triangleCount = 0;
            for (const auto& tc : triangles) {
                triangleCount += tc->numTriangles();
            }
        };

        updateHeightmap();
        volume->marchAsync(onMarchComplete, nodeObserver);
    }

    void updateHeightmap()
    {
        const auto minZ = 0.5F;
        const auto zRange = maxTerrainHeight - minZ;
        const auto zOffset = idx * size;
        const auto dim = size + 1;

        int slices = threadPool->size();
        int sliceHeight = dim / slices;
        std::vector<std::future<void>> workers;
        for (int slice = 0; slice < slices; slice++) {
            workers.push_back(threadPool->enqueue([=](int tIdx) {
                int sliceStart = slice * sliceHeight;
                int sliceEnd = sliceStart + sliceHeight;
                if (slice == slices - 1) {
                    sliceEnd = dim;
                }

                for (int y = sliceStart; y < sliceEnd; y++) {
                    for (int x = 0; x < dim; x++) {
                        float n = noise.GetSimplex(x, y + zOffset) * 0.5F + 0.5F;

                        // sand-dune like structure
                        float dune = n * 5;
                        dune = dune - floor(dune);
                        dune = dune * dune * zRange;

                        // floor
                        float f = n * n * n;
                        float floor = f * zRange;

                        heightmap[y * size + x] = minZ + floor + dune;
                    }
                }
            }));
        }

        for (auto& w : workers) {
            w.wait();
        }
    }

    const float maxTerrainHeight = 8.0F;

    int idx = 0;
    int size = 0;
    std::shared_ptr<mc::util::ThreadPool> threadPool;
    FastNoise& noise;
    int triangleCount;
    std::unique_ptr<mc::OctreeVolume> volume;
    std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>> triangles;
    mc::util::LineSegmentBuffer aabbLineBuffer;
    mc::util::LineSegmentBuffer boundingLineBuffer;
    mc::util::LineSegmentBuffer waypointLineBuffer;
    std::vector<vec3> waypoints;
    std::vector<float> heightmap;
    double lastMarchDurationSeconds = 0;

private:
    std::vector<vec4> _nodeColors;

    vec4 rainbow(float dist) const
    {
        using namespace mc::util::color;
        const hsv hC { 360 * dist, 0.6F, 1.0F };
        const auto rgbC = Hsv2Rgb(hC);
        return vec4(rgbC.r, rgbC.g, rgbC.b, 1);
    }

    vec4 nodeColor(int atDepth)
    {
        using namespace mc::util::color;

        auto depth = volume->depth();
        if (_nodeColors.size() < depth) {
            _nodeColors.clear();
            const float hueStep = 360.0F / depth;
            for (auto i = 0U; i <= depth; i++) {
                const hsv hC { i * hueStep, 0.6F, 1.0F };
                const auto rgbC = Hsv2Rgb(hC);
                _nodeColors.emplace_back(rgbC.r, rgbC.g, rgbC.b, mix<float>(0.6, 0.25, static_cast<float>(i) / depth));
            }
        }

        return _nodeColors[atDepth];
    }
};

#endif