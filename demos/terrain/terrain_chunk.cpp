#include "terrain_chunk.hpp"

#include "materials.hpp"
#include "terrain_samplers.hpp"
#include "xorshift.hpp"

using namespace glm;

namespace {
const float kMaxTerrainHeight = 8.0F;

float smoothstep(float t)
{
    return t * t * (3 - 2 * t);
}

float smoothstep(float edge0, float edge1, float t)
{
    t = (t - edge0) / (edge1 - edge0);
    return t * t * (3 - 2 * t);
}

vec4 rainbow(float dist)
{
    using namespace mc::util::color;
    const hsv hC { 360 * dist, 0.6F, 1.0F };
    const auto rgbC = Hsv2Rgb(hC);
    return vec4(rgbC.r, rgbC.g, rgbC.b, 1);
}
}

TerrainChunk::TerrainChunk(int size, std::shared_ptr<mc::util::ThreadPool> threadPool, FastNoise& noise)
    : _idx(-1)
    , _size(size)
    , _threadPool(threadPool)
    , _noise(noise)
{
    std::vector<mc::util::unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
    for (size_t i = 0, N = threadPool->size(); i < N; i++) {
        _triangles.push_back(std::make_unique<mc::TriangleConsumer<mc::Vertex>>());
        unownedTriangleConsumers.push_back(_triangles.back().get());
    }

    const int minNodeSize = 4;
    const float fuzziness = 2.0F;
    _volume = std::make_unique<mc::OctreeVolume>(size, fuzziness, minNodeSize, threadPool, unownedTriangleConsumers);

    //  +1 gives us the edge case for marching
    _heightmap.resize((size + 1) * (size + 1));
}

void TerrainChunk::build(int idx)
{
    using namespace glm;

    this->_idx = idx;
    _volume->clear();
    _triangleCount = 0;
    for (auto& tc : _triangles) {
        tc->clear();
    }

    _waypoints.clear();
    _waypointLineBuffer.clear();

    const auto segmentColor = rainbow(static_cast<float>(idx % 5) / 5.0F);

    //
    // build terrain sampler
    //

    _volume->add(std::make_unique<HeightmapSampler>(_heightmap.data(), _size, kMaxTerrainHeight, kFloorThreshold,
        kFloorTerrainMaterial, kLowTerrainMaterial, kHighTerrainMaterial));

    //
    //  Build an RNG seeded for this segment
    //

    auto rng = rng_xorshift64 { static_cast<uint64_t>(12345 * (idx + 1)) };

    //
    // build arches
    //

    const float zOffset = idx * _size;
    const auto size = vec3(_volume->size());
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
        float archZ = 30 + (_size - 60) * static_cast<float>(i) / maxArches;
        float archX = center.x + _noise.GetSimplex(archZ + zOffset, 0) * size.x * 0.125F;

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
        arch.material = kArchMaterial;

        _volume->add(std::make_unique<Tube>(arch));

        vec3 waypoint = arch.axisOrigin + arch.axisPerp * arch.innerRadius * rng.nextFloat(0.2F, 0.8F);
        waypoint.y = std::max(waypoint.y, kMaxTerrainHeight + 2);
        _waypoints.push_back(waypoint);
        _waypointLineBuffer.addMarker(waypoint, 4, segmentColor);
    }

    // all segments have to have at least 1 waypoint; if the dice roll
    // didn't give us any arches, make a default waypoint
    if (_waypoints.empty()) {
        vec3 waypoint = vec3(center.x, kMaxTerrainHeight + rng.nextFloat(10), center.z);
        _waypoints.push_back(waypoint);
        _waypointLineBuffer.addMarker(waypoint, 4, vec4(1, 1, 0, 1));
    }

    //
    //  Build a debug frame to show our volume
    //

    _boundingLineBuffer.clear();
    _boundingLineBuffer.add(AABB(vec3 { 0.0F }, size).inset(1), segmentColor);
}

void TerrainChunk::march()
{
    const double startTime = glfwGetTime();

    _aabbLineBuffer.clear();

    const auto nodeObserver = [this](mc::OctreeVolume::Node* node) {
        {
            // update the occupied aabb display
            auto bounds = node->bounds;
            bounds.inset(node->depth * 0.005F);
            _aabbLineBuffer.add(bounds, nodeColor(node->depth));
        }
    };

    const auto onMarchComplete = [this, startTime]() {
        _lastMarchDurationSeconds = glfwGetTime() - startTime;

        _triangleCount = 0;
        for (const auto& tc : _triangles) {
            _triangleCount += tc->numTriangles();
        }
    };

    updateHeightmap();
    _volume->marchAsync(onMarchComplete, nodeObserver);
}

void TerrainChunk::updateHeightmap()
{
    _isUpdatingHeightmap = true;
    const auto zOffset = _idx * _size;
    const auto dim = _size + 1;

    int slices = _threadPool->size();
    int sliceHeight = dim / slices;
    std::vector<std::future<void>> workers;
    for (int slice = 0; slice < slices; slice++) {
        workers.push_back(_threadPool->enqueue([=](int tIdx) {
            int sliceStart = slice * sliceHeight;
            int sliceEnd = sliceStart + sliceHeight;
            if (slice == slices - 1) {
                sliceEnd = dim;
            }

            for (int y = sliceStart; y < sliceEnd; y++) {
                for (int x = 0; x < dim; x++) {
                    float n = _noise.GetSimplex(x, y + zOffset) * 0.5F + 0.5F;

                    // sand-dune like structures
                    float dune = n * 3;
                    dune = dune - floor(dune);
                    dune = dune * dune * kMaxTerrainHeight;

                    float dune2 = n * 2;
                    dune2 = dune2 - floor(dune2);
                    dune2 = smoothstep(dune2) * kMaxTerrainHeight;

                    // floor
                    float f = smoothstep(n);
                    float floor = kMaxTerrainHeight * f;

                    _heightmap[y * _size + x] = (floor + dune + dune2) / 3.0F;
                }
            }
        }));
    }

    for (auto& w : workers) {
        w.wait();
    }
    _isUpdatingHeightmap = false;
}

vec4 TerrainChunk::nodeColor(int atDepth)
{
    using namespace mc::util::color;

    auto depth = _volume->depth();
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
