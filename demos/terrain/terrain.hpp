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

#include "../common/xorshift.hpp"
#include "FastNoise.h"
#include "terrain_samplers.hpp"

class GreebleSource {
public:
    struct Sample {
        float probability;
        glm::vec3 offset;
        uint64_t seed;
    };

public:
    GreebleSource() = default;
    virtual ~GreebleSource() = default;
    virtual int sampleStepSize() const = 0;
    virtual Sample sample(const vec3 world) const = 0;
    virtual std::unique_ptr<mc::IVolumeSampler> evaluate(const Sample& sample, const vec3& local) const = 0;
};

struct TerrainChunk {
public:
    /**
     * Create a cube of terrain, where size is the size of an edge of the cube.
     */
    TerrainChunk(int size, mc::util::unowned_ptr<TerrainSampler::SampleSource> terrain);

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
    mc::util::unowned_ptr<TerrainSampler::SampleSource> _terrainSampleSource;
    std::unique_ptr<mc::OctreeVolume> _volume;
    mc::util::unowned_ptr<TerrainSampler> _groundSampler;
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
    TerrainGrid(int gridSize, int chunkSize,
        std::unique_ptr<TerrainSampler::SampleSource>&& terrainSampleSource,
        std::unique_ptr<GreebleSource>&& greebler);

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

private:
    /**
     * Returns true if the sampler intersects the given bounds in world coordinates. Note, samplers
     * exist in the coordinate system of a specific volume, so to work, this method requires
     * samplerChunkWorldOrigin which is the origin of the terrain chunk the sampler is in.
     */
    bool samplerIntersects(mc::IVolumeSampler* sampler, const vec3& samplerChunkWorldOrigin, const AABB worldBounds);

    void updateGreebling();
    void marchSerially();

private:
    int _gridSize = 0;
    int _chunkSize = 0;
    int _centerOffset = 0;
    bool _isMarching = false;
    std::vector<std::unique_ptr<TerrainChunk>> _grid;
    std::vector<TerrainChunk*> _dirtyChunks;
    std::unique_ptr<TerrainSampler::SampleSource> _terrainSampleSource;
    std::unique_ptr<GreebleSource> _greebleSource;
};

#endif