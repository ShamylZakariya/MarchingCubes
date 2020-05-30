#include "terrain.hpp"

#include <glm/ext.hpp>
#include <glm/gtc/noise.hpp>

#include "../common/xorshift.hpp"
#include "materials.hpp"
#include "terrain_samplers.hpp"

using namespace glm;

namespace {
const float kFloorThreshold = 1.0F;

vec4 rainbow(float dist)
{
    using namespace mc::util::color;
    const hsv hC { 360 * dist, 0.6F, 1.0F };
    const auto rgbC = Hsv2Rgb(hC);
    return vec4(rgbC.r, rgbC.g, rgbC.b, 1);
}

vec4 nodeColor(int atDepth)
{
    return rainbow((atDepth % 8) / 8.0F);
}

}

TerrainChunk::TerrainChunk(int size, TerrainVolumeSampler terrainFn, float maxHeight)
    : _index(0, 0)
    , _size(size)
    , _maxHeight(maxHeight)
    , _threadPool(std::thread::hardware_concurrency(), true)
    , _terrainVolume(terrainFn)
{
    std::vector<mc::util::unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
    for (size_t i = 0, N = _threadPool.size(); i < N; i++) {
        _triangles.push_back(std::make_unique<mc::TriangleConsumer<mc::Vertex>>());
        unownedTriangleConsumers.push_back(_triangles.back().get());
    }

    const int minNodeSize = 4;
    const float fuzziness = 2.0F;
    _volume = std::make_unique<mc::OctreeVolume>(size, fuzziness, minNodeSize, &_threadPool, unownedTriangleConsumers);
}

void TerrainChunk::setIndex(ivec2 index)
{
    _needsMarch = true;
    _index = index;
    _volume->clear();
    for (auto& tc : _triangles) {
        tc->clear();
    }

    const auto xzOffset = getXZOffset();
    const auto size = vec3(_volume->size());
    const auto worldOrigin = vec3(xzOffset.x, 0, xzOffset.y);

    // bounds are in world not local space
    _bounds = AABB(worldOrigin, worldOrigin + size);

    // build a ground sampling volume
    auto noise = [this, xzOffset](const vec3& world) -> float {
        auto s = world + vec3(xzOffset.x, 0, xzOffset.y);
        return _terrainVolume(s);
    };

    _groundSampler = _volume->add(std::make_unique<GroundSampler>(noise, _maxHeight, kFloorThreshold,
        kFloorTerrainMaterial, kLowTerrainMaterial, kHighTerrainMaterial));

    //  Build a debug frame to show our volume

    const auto segmentColor = rainbow(static_cast<float>((_index.x * 50 + _index.y) % 10) / 10.0F);
    _boundingLineBuffer.clear();
    _boundingLineBuffer.add(AABB(vec3 { 0.0F }, size).inset(1), segmentColor);
}

void TerrainChunk::march(std::function<void()> onComplete)
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

    const auto onMarchComplete = [this, startTime, onComplete]() {
        _lastMarchDurationSeconds = glfwGetTime() - startTime;
        onComplete();
        _isMarching = false;
        _needsMarch = false;
    };

    _isMarching = true;
    _volume->marchAsync(onMarchComplete, nodeObserver);
}

///////////////////////////////////////////////////////////////////////////////

namespace {
int makeOdd(int v)
{
    if (v % 2)
        return v;
    return v + 1;
}
}

TerrainGrid::TerrainGrid(int gridSize, int chunkSize, TerrainVolumeSampler terrainFn, float maxHeight, GreebleSampler greebleSampler)
    : _gridSize(makeOdd(gridSize))
    , _chunkSize(chunkSize)
    , _greebleSampler(greebleSampler)
{
    _grid.resize(_gridSize * _gridSize);
    for (int i = 0; i < _gridSize; i++) {
        for (int j = 0; j < _gridSize; j++) {
            int k = i * _gridSize + j;
            _grid[k] = std::make_unique<TerrainChunk>(chunkSize, terrainFn, maxHeight);
            _grid[k]->setIndex(ivec2(j - _gridSize / 2, i - _gridSize / 2));
        }
    }
    _centerOffset = (_gridSize * _gridSize) / 2;
}

glm::ivec2 TerrainGrid::worldToIndex(const glm::vec3& world) const
{
    auto idx = ivec2(world.x / _chunkSize, world.z / _chunkSize);
    if (world.x < 0)
        idx.x--;
    if (world.z < 0)
        idx.y--;
    return idx;
}

void TerrainGrid::shift(glm::ivec2 by)
{
    // shift dx
    int dx = by.x;
    while (dx != 0) {
        if (dx > 0) {
            // shift contents to right, recycling rightmost column to left
            for (int y = 0; y < _gridSize; y++) {
                int rowOffset = y * _gridSize;
                for (int x = _gridSize - 1; x > 0; x--) {
                    std::swap(_grid[rowOffset + x], _grid[rowOffset + x - 1]);
                }
                _grid[rowOffset]->setIndex(_grid[rowOffset + 1]->getIndex() + ivec2(-1, 0));
            }
            dx--;
        } else {
            // WORKS
            // shift contents to left, recycling elements at left side of each row to right
            for (int y = 0; y < _gridSize; y++) {
                int rowOffset = y * _gridSize;
                for (int x = 0; x < _gridSize - 1; x++) {
                    std::swap(_grid[rowOffset + x], _grid[rowOffset + x + 1]);
                }
                _grid[rowOffset + _gridSize - 1]->setIndex(_grid[rowOffset + _gridSize - 2]->getIndex() + ivec2(+1, 0));
            }
            dx++;
        }
    }

    // shift dy
    int dy = by.y;
    while (dy != 0) {
        if (dy > 0) {
            // shift contents down, recycling elements at bottom to top
            for (int x = 0; x < _gridSize; x++) {
                for (int y = _gridSize - 1; y > 0; y--) {
                    std::swap(_grid[y * _gridSize + x], _grid[((y - 1) * _gridSize) + x]);
                }
                _grid[x]->setIndex(_grid[_gridSize + x]->getIndex() + ivec2(0, -1));
            }
            dy--;
        } else {
            // shift contents up, recycling elements at top to bottom
            for (int x = 0; x < _gridSize; x++) {
                for (int y = 0; y < _gridSize - 1; y++) {
                    std::swap(_grid[y * _gridSize + x], _grid[(y + 1) * _gridSize + x]);
                }
                _grid[(_gridSize - 1) * _gridSize + x]->setIndex(_grid[(_gridSize - 2) * _gridSize + x]->getIndex() + ivec2(0, 1));
            }
            dy++;
        }
    }
}

void TerrainGrid::print()
{
    std::cout << "TerrainGrid::print\n";
    for (int i = 0; i < _gridSize; i++) {
        for (int j = 0; j < _gridSize; j++) {
            int k = i * _gridSize + j;
            std::cout << "\t" << glm::to_string(_grid[k]->getIndex()) << std::endl;
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

void TerrainGrid::march(const glm::vec3& viewPos, const glm::vec3& viewDir)
{
    // collect all TerrainChunk instances which need to be marched, and aren't being marched
    _dirtyChunks.clear();
    for (const auto& chunk : _grid) {
        if (chunk->needsMarch() && !chunk->isWorking()) {
            _dirtyChunks.push_back(chunk.get());
        }
    }

    // sort such that the elements most in front of view are at end of vector
    std::sort(_dirtyChunks.begin(), _dirtyChunks.end(), [&viewPos, &viewDir](TerrainChunk* a, TerrainChunk* b) -> bool {
        vec3 vToA = normalize(a->getBounds().center() - viewPos);
        vec3 vToB = normalize(b->getBounds().center() - viewPos);
        float da = dot(vToA, viewDir);
        float db = dot(vToB, viewDir);
        return da < db;
    });

    // now march the queue serially from back to front
    if (!_dirtyChunks.empty()) {
        _isMarching = true;
        updateGreebling();
        marchSerially();
    }
}

void TerrainGrid::findChunksIntersecting(AABB region, std::vector<mc::util::unowned_ptr<TerrainChunk>>& into)
{
    for (const auto& chunk : _grid) {
        if (chunk->getBounds().intersect(region) != AABB::Intersection::Outside) {
            into.push_back(chunk.get());
        }
    }
}

void TerrainGrid::findChunksIntersecting(mc::IVolumeSampler* sampler, const vec3& samplerChunkWorldOrigin, std::vector<mc::util::unowned_ptr<TerrainChunk>>& into)
{
    for (const auto& chunk : _grid) {
        const auto relativeOrigin = chunk->getWorldOrigin() - samplerChunkWorldOrigin;
        const auto relativeBounds = AABB{ relativeOrigin, relativeOrigin + chunk->getBounds().size() };
        if (sampler->intersects(relativeBounds)) {
            into.push_back(chunk);
        }
    }
}

void TerrainGrid::updateGreebling()
{
    std::vector<mc::util::unowned_ptr<TerrainChunk>> chunks;

    for (const auto& chunk : _dirtyChunks) {
        std::cout << "Greebling chunk " << glm::to_string(chunk->getIndex()) << std::endl;

        auto bounds = chunk->getBounds();
        auto center = bounds.size() / 2;
        auto position = vec3{0, center.y, center.z};

        auto shape = std::make_unique<mc::SphereVolumeSampler>(position, 20, mc::IVolumeSampler::Mode::Additive);
        chunks.clear();
        findChunksIntersecting(shape.get(), chunk->getWorldOrigin(), chunks);

        if (chunks.size() == 1) {
            std::cout << "\tInserting shape " << shape.get() << " into SINGLE chunk " << glm::to_string(chunk->getIndex()) << std::endl;
            chunks.front()->getVolume()->add(std::move(shape));
        } else if (chunks.size() > 1) {
            for (const auto& spanningChunk : chunks) {
                std::cout << "\tInserting shape " << shape.get() << " into SPANNING chunk " << glm::to_string(chunk->getIndex()) << std::endl;
                // create a translation to apply to the clone so that when inserted into spanning chunk, it will
                // be in the right local position to map to the same world position as the source chunk.
                const auto translation = chunk->getWorldOrigin() - spanningChunk->getWorldOrigin();
                auto clone = shape->copy();
                clone->translate(translation);
                spanningChunk->getVolume()->add(std::move(clone));
            }
        }
    }

    // for (const auto& chunk : _dirtyChunks) {
    //     const auto bounds = chunk->getBounds();
    //     const auto stepSize = bounds.size() / 10.0F;
    //     for (float x = bounds.min.x; x < bounds.max.x; x += stepSize.x) {
    //         for (float z = bounds.min.z; z < bounds.max.z; z += stepSize.z) {
    //             const GreebleSample greeble = _greebleSampler(vec2(floor(x), floor(z)));
    //             if (greeble.probability > 0.7) {
    //                 auto rng = rng_xorshift64 { greeble.seed };
    //                 // create an arch

    //                 Tube::Config arch;
    //                 arch.axisOrigin = vec3 { x + greeble.offset.x, 0, z + greeble.offset.y };
    //                 arch.innerRadiusAxisOffset = vec3(0, rng.nextFloat(4, 10), 0);
    //                 arch.axisDir = normalize(vec3(rng.nextFloat(-0.6, 0.6), rng.nextFloat(-0.2, 0.2), 1));
    //                 arch.axisPerp = normalize(vec3(rng.nextFloat(-0.2, 0.2), 1, 0));
    //                 arch.length = rng.nextFloat(7, 11);
    //                 arch.innerRadius = rng.nextFloat(35, 43);
    //                 arch.outerRadius = rng.nextFloat(48, 55);
    //                 arch.frontFaceNormal = arch.axisDir;
    //                 arch.backFaceNormal = -arch.axisDir;
    //                 arch.cutAngleRadians = radians(rng.nextFloat(16, 32));
    //                 arch.material = kArchMaterial;

    //                 auto tubeProto = std::make_unique<Tube>(arch);
    //                 chunks.clear();
    //                 findChunksIntersecting(tubeProto.get(), chunks);

    //                 if (chunks.size() == 1) {
    //                     chunks.front()->getVolume()->add(std::move(tubeProto));
    //                 } else if (chunks.size() > 1) {
    //                     for (const auto& chunk : chunks) {
    //                         chunk->getVolume()->add(tubeProto->copy());
    //                     }
    //                 }

    //             } else if (greeble.probability < 0.3) {
    //                 // create something else? A boulder? A tower?
    //             }
    //         }
    //     }
    // }
}

void TerrainGrid::marchSerially()
{
    _dirtyChunks.back()->march([this]() {
        _dirtyChunks.pop_back();
        if (!_dirtyChunks.empty()) {
            marchSerially();
        } else {
            _isMarching = false;
        }
    });
}
