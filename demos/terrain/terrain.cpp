#include "terrain.hpp"

#include <glm/ext.hpp>

#include "materials.hpp"
#include "terrain_samplers.hpp"
#include "xorshift.hpp"

using namespace glm;

namespace {
const float kFloorThreshold = 1.0F;
const float kMaxTerrainHeight = 16.0F;

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

vec4 nodeColor(int atDepth)
{
    return rainbow((atDepth % 8) / 8.0F);
}

}

TerrainChunk::TerrainChunk(int size, FastNoise& noise)
    : _index(0, 0)
    , _size(size)
    , _threadPool(std::thread::hardware_concurrency(), true)
    , _noise(noise)
{
    std::vector<mc::util::unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
    for (size_t i = 0, N = _threadPool.size(); i < N; i++) {
        _triangles.push_back(std::make_unique<mc::TriangleConsumer<mc::Vertex>>());
        unownedTriangleConsumers.push_back(_triangles.back().get());
    }

    const int minNodeSize = 4;
    const float fuzziness = 2.0F;
    _volume = std::make_unique<mc::OctreeVolume>(size, fuzziness, minNodeSize, &_threadPool, unownedTriangleConsumers);

    //  +1 gives us the edge case for marching
    _heightmap.resize((size + 1) * (size + 1));
}

void TerrainChunk::setIndex(ivec2 index)
{
    _index = index;
    _volume->clear();
    _triangleCount = 0;
    for (auto& tc : _triangles) {
        tc->clear();
    }

    //
    // build terrain sampler
    //

    _volume->add(std::make_unique<HeightmapSampler>(_heightmap.data(), _size, kMaxTerrainHeight, kFloorThreshold,
        kFloorTerrainMaterial, kLowTerrainMaterial, kHighTerrainMaterial));

    const auto xzOffset = getXZOffset();
    const auto size = vec3(_volume->size());

    //
    //  Build the bounds in world space
    //

    const auto worldOrigin = vec3(xzOffset.x, 0, xzOffset.y);
    _bounds = AABB(worldOrigin, worldOrigin + size);

    //
    //  Build a debug frame to show our volume
    //

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
        for (const auto& tc : _triangles) {
            _triangleCount += tc->numTriangles();
        }

        std::cout << "TerrainChunk[" << glm::to_string(getIndex()) << "]::march - complete _triangleCount: " << _triangleCount << std::endl;
        onComplete();
    };

    _isPreparingForMarch = true;
    updateHeightmap();
    _volume->marchAsync(onMarchComplete, nodeObserver);
    _isPreparingForMarch = false;
}

void TerrainChunk::updateHeightmap()
{
    const auto xzOffset = getXZOffset();
    const auto dim = _size + 1;

    int slices = _threadPool.size();
    int sliceHeight = dim / slices;
    std::vector<std::future<void>> workers;
    for (int slice = 0; slice < slices; slice++) {
        workers.push_back(_threadPool.enqueue([=](int tIdx) {
            int sliceStart = slice * sliceHeight;
            int sliceEnd = sliceStart + sliceHeight;
            if (slice == slices - 1) {
                sliceEnd = dim;
            }

            for (int y = sliceStart; y < sliceEnd; y++) {
                for (int x = 0; x < dim; x++) {
                    float n = _noise.GetSimplex(x + xzOffset.x, y + xzOffset.y) * 0.5F + 0.5F;

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

TerrainGrid::TerrainGrid(int gridSize, int chunkSize, FastNoise& noise)
    : _gridSize(makeOdd(gridSize))
    , _chunkSize(chunkSize)
{
    _grid.resize(_gridSize * _gridSize);
    for (int i = 0; i < _gridSize; i++) {
        for (int j = 0; j < _gridSize; j++) {
            int k = i * _gridSize + j;
            _grid[k] = std::make_unique<TerrainChunk>(chunkSize, noise);
            _grid[k]->setIndex(ivec2(j - _gridSize / 2, i - _gridSize / 2));
        }
    }
}

void TerrainGrid::shift(glm::ivec2 by)
{
    // shift dx
    int dx = by.x;
    while (dx != 0) {
        if (dx > 0) {
            // shift contents to right, recycling elements at right side of each row to left
            for (int y = 0; y < _gridSize; y++) {
                int rowOffset = y * _gridSize;
                for (int x = _gridSize - 1; x >= 0; x--) {
                    std::swap(_grid[rowOffset + x], _grid[rowOffset + x - 1]);
                }
                _grid[rowOffset]->setIndex(_grid[rowOffset + 1]->getIndex() + ivec2(-1, 0));
            }
            dx--;
        } else {
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
    if (by.y != 0) {
        if (dy > 0) {
            // shift contents down, recycling elements at bottom to top
            for (int x = 0; x < _gridSize; x++) {
                for (int y = _gridSize - 1; y >= 0; y--) {
                    std::swap(_grid[y * _gridSize + x], _grid[y * (_gridSize - 1) + x]);
                }
                _grid[x]->setIndex(_grid[_gridSize + x]->getIndex() + ivec2(0, -1));
            }
            dx--;
        } else {
            // shift contents up, recycling elements at top to bottom
            for (int x = 0; x < _gridSize; x++) {
                for (int y = 0; y < _gridSize - 1; y++) {
                    std::swap(_grid[y * _gridSize + x], _grid[(y + 1) * _gridSize + x]);
                }
                _grid[(_gridSize - 1) * _gridSize + x]->setIndex(_grid[(_gridSize - 2) * _gridSize + x]->getIndex() + ivec2(0, 1));
            }
            dx++;
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

namespace {

    void marchSerially(std::vector<TerrainChunk*> &chunks) {
        if (!chunks.empty()) {
            chunks.back()->march([&chunks](){
                chunks.pop_back();
                marchSerially(chunks);
            });
        }
    }
}

void TerrainGrid::march(const glm::vec3& viewPos, const glm::vec3& viewDir)
{
    // collect all TerrainChunk which needsMarch
    _chunksToMarch.clear();
    for (const auto& chunk : _grid) {
        if (chunk->needsMarch()) {
            _chunksToMarch.push_back(chunk.get());
        }
    }

    // sort such that the elements most in front of view are at end of vector
    std::sort(_chunksToMarch.begin(), _chunksToMarch.end(), [&viewPos, &viewDir](TerrainChunk* a, TerrainChunk* b) -> bool {
        vec3 vToA = normalize(a->getBounds().center() - viewPos);
        vec3 vToB = normalize(b->getBounds().center() - viewPos);
        float da = dot(vToA, viewDir);
        float db = dot(vToB, viewDir);
        return da < db;
    });

    marchSerially(_chunksToMarch);
}
