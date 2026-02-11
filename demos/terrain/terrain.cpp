#include "terrain.hpp"

#include <glm/ext.hpp>
#include <glm/gtc/noise.hpp>

#include "terrain_samplers.hpp"

using namespace glm;

namespace {

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

TerrainChunk::TerrainChunk(int size, mc::util::unowned_ptr<TerrainSampler::SampleSource> terrain)
    : _index(0, 0)
    , _size(size)
    , _maxHeight(terrain->maxHeight())
    , _threadPool(std::thread::hardware_concurrency(), true)
    , _terrainSampleSource(terrain)
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
    const auto size = vec3(_volume->getSize());
    const auto sampleOffset = vec3(xzOffset.x, 0, xzOffset.y);

    // bounds are in world not local space
    _bounds = AABB(sampleOffset, sampleOffset + size);
    _groundSampler = _volume->add(std::make_unique<TerrainSampler>(_terrainSampleSource, sampleOffset));

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

TerrainGrid::TerrainGrid(int gridSize, int chunkSize,
    std::unique_ptr<TerrainSampler::SampleSource>&& terrainSampleSource,
    std::unique_ptr<GreebleSource>&& greebleSource)
    : _gridSize(makeOdd(gridSize))
    , _chunkSize(chunkSize)
    , _terrainSampleSource(std::move(terrainSampleSource))
    , _greebleSource(std::move(greebleSource))
{
    _grid.resize(_gridSize * _gridSize);
    for (int i = 0; i < _gridSize; i++) {
        for (int j = 0; j < _gridSize; j++) {
            int k = i * _gridSize + j;
            _grid[k] = std::make_unique<TerrainChunk>(chunkSize, _terrainSampleSource.get());
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

mc::util::unowned_ptr<TerrainChunk> TerrainGrid::getTerrainChunkContaining(const glm::vec3& world) const
{
    const ivec2 idx0 = _grid[0]->getIndex();
    const ivec2 targetIdx = worldToIndex(world);
    const ivec2 delta = targetIdx - idx0;
    if (delta.x >= 0 && delta.x < _gridSize && delta.y >= 0 && delta.y < _gridSize) {
        return _grid[delta.y * _gridSize + delta.x];
    }
    return nullptr;
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
            std::cout << "\tidx:" << k << "\t" << glm::to_string(_grid[k]->getIndex()) << std::endl;
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

namespace {
const float kIsoThreshold = 0.001F;
vec3 normalAt(mc::OctreeVolume::Node* node, glm::vec3 p)
{
    mc::MaterialState _;
    const float d = 0.05f;
    vec3 gradient(
        node->valueAt(p + vec3(d, 0, 0), 1, _, false) - node->valueAt(p + vec3(-d, 0, 0), 1, _, false),
        node->valueAt(p + vec3(0, d, 0), 1, _, false) - node->valueAt(p + vec3(0, -d, 0), 1, _, false),
        node->valueAt(p + vec3(0, 0, d), 1, _, false) - node->valueAt(p + vec3(0, 0, -d), 1, _, false));

    return -normalize(gradient);
}

}

TerrainGrid::RaycastResult TerrainGrid::rayCast(const glm::vec3& origin, const glm::vec3& dir,
    float stepSize, float maxLength,
    bool computeNormal, RaycastEdgeBehavior edgeBehavior) const
{
    const float maxLength2 = maxLength * maxLength;
    const float minStepSize = stepSize * pow<float>(0.5, 6);
    const bool clamp = edgeBehavior == RaycastEdgeBehavior::Clamp;
    mc::util::unowned_ptr<TerrainChunk> currentChunk = getTerrainChunkContaining(origin);
    mc::util::unowned_ptr<TerrainChunk> lastChunk = getTerrainChunkContaining(origin + dir * maxLength);
    const bool crossesChunks = lastChunk != currentChunk;
    vec3 samplePoint = origin;
    mc::MaterialState _;
    bool firstStep = true;
    bool forward = true;
    bool wasInsideVolume = false;

    while (distance2(samplePoint, origin) < maxLength2) {
        auto volume = currentChunk->getVolume();
        auto chunkWorldOrigin = currentChunk->getWorldOrigin();
        vec3 localSamplePoint = samplePoint - chunkWorldOrigin;
        if (clamp) {
            localSamplePoint = volume->getBounds().clamp(localSamplePoint);
        }

        auto node = volume->findNode(localSamplePoint);
        if (node != nullptr) {
            float value = node->valueAt(localSamplePoint, 1, _, false);

            if (firstStep && value > 0.5F + kIsoThreshold) {
                // the raycast origin is inside the volume. Reverse raycast to find exit point.
                forward = false;
                wasInsideVolume = true;
                stepSize *= -1;
            }

            // we landed right on the iso surface boundary, we're done.
            if (std::abs(value - 0.5F) < kIsoThreshold) {
                RaycastResult result;
                result.isHit = true;
                result.distance = glm::distance(samplePoint, origin) * (wasInsideVolume ? -1 : 1);
                result.position = samplePoint;
                if (computeNormal) {
                    result.normal = normalAt(node, localSamplePoint);
                }
            }

            if (forward) {
                if (value > 0.5F) {
                    forward = false;
                    stepSize *= -0.5F;
                }
            } else {
                if (value < 0.5F) {
                    forward = true;
                    stepSize *= -0.5F;
                }
            }
        }

        if (std::abs(stepSize) <= minStepSize) {
            // we've been binary searching about the threshold; this is good enough.
            RaycastResult result;
            result.isHit = true;
            result.position = samplePoint;
            result.distance = glm::distance(samplePoint, origin) * (wasInsideVolume ? -1 : 1);
            if (computeNormal) {
                result.normal = normalAt(node, localSamplePoint);
            }
            return result;
        }

        samplePoint += dir * stepSize;
        firstStep = false;

        if (crossesChunks) {
            currentChunk = getTerrainChunkContaining(samplePoint);
            if (!currentChunk) {
                return RaycastResult::none();
            }
        }
    }

    std::cout << "Done - exceeded max raycast distance" << std::endl;
    return RaycastResult::none();
}

bool TerrainGrid::samplerIntersects(mc::IVolumeSampler* sampler, const vec3& samplerChunkWorldOrigin, const AABB worldBounds)
{
    const auto relativeOrigin = worldBounds.min - samplerChunkWorldOrigin;
    const auto relativeBounds = AABB { relativeOrigin, relativeOrigin + worldBounds.size() };
    return sampler->intersects(relativeBounds);
}

namespace {
float snap(float v, int step)
{
    int x = v / step;
    return x * step;
}
}

void TerrainGrid::updateGreebling()
{
    if (!_greebleSource)
        return;

    const int step = _greebleSource->sampleStepSize();
    for (const auto& chunk : _dirtyChunks) {
        const auto chunkBounds = chunk->getBounds();
        const auto extent = chunkBounds.size();
        const auto range = AABB(
            vec3(snap(chunkBounds.min.x - extent.x, step), chunkBounds.min.y, snap(chunkBounds.min.z - extent.z, step)),
            vec3(snap(chunkBounds.max.x + extent.x, step), chunkBounds.max.y, snap(chunkBounds.max.z + extent.z, step)));

        for (float x = range.min.x; x <= range.max.x; x += step) {
            for (float z = range.min.z; z <= range.max.z; z += step) {
                const vec3 world(x, 0, z);
                const GreebleSource::Sample sample = _greebleSource->sample(world);
                const vec3 local(world.x - chunkBounds.min.x, 0, world.z - chunkBounds.min.z);
                std::unique_ptr<mc::IVolumeSampler> greeble = _greebleSource->evaluate(sample, local);
                if (greeble) {
                    chunk->getVolume()->add(std::move(greeble));
                }
            }
        }
    }
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
