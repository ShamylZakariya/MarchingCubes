#ifndef terrain_segment_hpp
#define terrain_segment_hpp

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <mc/marching_cubes.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "FastNoise.h"
#include "terrain_samplers.hpp"

typedef std::function<float(const vec3&)> TerrainVolumeSampler;

struct GreebleSample {
    float probability;
    glm::vec2 offset;
    uint64_t seed;
};
typedef std::function<GreebleSample(const vec2&)> GreebleSampler;

const mc::MaterialState kFloorTerrainMaterial {
    glm::vec4(0, 0, 0, 1),
    1,
    0,
    0
};

const mc::MaterialState kLowTerrainMaterial {
    glm::vec4(1, 1, 1, 1),
    0,
    1,
    0
};

const mc::MaterialState kHighTerrainMaterial {
    glm::vec4(0.3, 0.3, 0.3, 1),
    0,
    1,
    0
};

const mc::MaterialState kArchMaterial {
    glm::vec4(0.1, 0.2, 0.1, 1),
    0.125,
    0,
    1
};

struct TerrainChunk {
public:
    /**
     * Create a cube of terrain, where size is the size of an edge of the cube.
     */
    TerrainChunk(int size, TerrainVolumeSampler terrainFn, float maxHeight);

    ~TerrainChunk() = default;
    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk(TerrainChunk&&) = delete;
    TerrainChunk& operator==(const TerrainChunk&) = delete;

    void setIndex(glm::ivec2 index);
    glm::ivec2 getIndex() const { return _index; }

    bool needsMarch() const { return _needsMarch; }

    /**
     * Kicks off geometry generation, calling onComplete when the
     */
    void march(std::function<void()> onComplete);

    /**
     * Returns true if this segment is busy (generating heightmap, or marching the corresponding volume)
     */
    bool isWorking() const { return _isMarching; }

    mc::util::AABB getBounds() const { return _bounds; }
    mc::util::unowned_ptr<mc::OctreeVolume> getVolume() const { return _volume.get(); }
    const std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>>& getGeometry() const { return _triangles; }
    mc::util::LineSegmentBuffer& getAabbLineBuffer() { return _aabbLineBuffer; }
    mc::util::LineSegmentBuffer& getBoundingLineBuffer() { return _boundingLineBuffer; }
    double getLastMarchDurationSeconds() const { return _lastMarchDurationSeconds; }
    glm::vec3 getWorldOrigin() const { return glm::vec3(_index.x * _size, 0, _index.y * _size); }

private:
    glm::vec2 getXZOffset() const { return _index * _size; }

    glm::ivec2 _index;
    int _size = 0;
    float _maxHeight = 0;
    mc::util::ThreadPool _threadPool;
    mc::util::AABB _bounds;
    TerrainVolumeSampler _terrainVolume;
    std::unique_ptr<mc::OctreeVolume> _volume;
    mc::util::unowned_ptr<GroundSampler> _groundSampler;
    std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>> _triangles;
    mc::util::LineSegmentBuffer _aabbLineBuffer;
    mc::util::LineSegmentBuffer _boundingLineBuffer;
    double _lastMarchDurationSeconds = 0;
    bool _needsMarch = false;
    bool _isMarching = false;
};

class TerrainGrid {
public:
    /**
     * Create a terrain grid size*size
     */
    TerrainGrid(int gridSize, int chunkSize, TerrainVolumeSampler terrainFn, float terrainHeight, GreebleSampler greebler);

    /**
     * Convert a position in world space to the corresponding tile index.
     */
    glm::ivec2 worldToIndex(const glm::vec3& world) const;

    /**
     * Get the terrain chunk at the center of the grid
     */
    mc::util::unowned_ptr<TerrainChunk> getCenterChunk() const
    {
        return _grid[_centerOffset];
    }

    /**
     * Shift the grid of terrain chunks by a given amount. For example, shifting by (1,0) means
     * "shift right" by 1. Which will move each tile to the right, and recycle the rightmost set of
     * tiles to the left column, assign them the appriate indexes, and
     */
    void shift(glm::ivec2 by);

    void print();

    void forEach(std::function<void(mc::util::unowned_ptr<TerrainChunk>)> cb)
    {
        for (const auto& chunk : _grid) {
            cb(chunk.get());
        }
    }

    void march(const glm::vec3& viewPos, const glm::vec3& viewDir);

    int getGridSize() const { return _gridSize; }
    glm::vec3 getChunkSize() const { return glm::vec3(_chunkSize); }
    int getCount() const { return _gridSize * _gridSize; }
    bool isMarching() const { return _isMarching; }

    /**
     * Returns a list of TerrainChunks which intersect the provided AABB
     */
    std::vector<mc::util::unowned_ptr<TerrainChunk>> findChunksIntersecting(AABB region)
    {
        std::vector<mc::util::unowned_ptr<TerrainChunk>> store;
        findChunksIntersecting(region, store);
        return store;
    }

private:
    void findChunksIntersecting(AABB region, std::vector<mc::util::unowned_ptr<TerrainChunk>>& into);

    /**
     * Find terrain chunks which intersect the given sampler. Since samplers are defined in chunk-local space (not world)
     * this requires the world origin of a terrain chunk to act as a root coordinate system.
     */
    void findChunksIntersecting(mc::IVolumeSampler* sampler, const vec3& samplerChunkWorldOrigin, std::vector<mc::util::unowned_ptr<TerrainChunk>>& into);
    void updateGreebling();
    void marchSerially();

private:
    int _gridSize = 0;
    int _chunkSize = 0;
    int _centerOffset = 0;
    bool _isMarching = false;
    std::vector<std::unique_ptr<TerrainChunk>> _grid;
    std::vector<TerrainChunk*> _dirtyChunks;
    GreebleSampler _greebleSampler;
    std::map<glm::ivec2, std::vector<mc::IVolumeSampler*>> _greebleSamplersByGridIndex;
};

#endif