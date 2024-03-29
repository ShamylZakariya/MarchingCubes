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

// Represents a source of "greeble" detail for a TerrainGrid. For example, this could be
// used to add boulders, trees, etc to a TerrainGrid. Note: the added details should be small,
// relative to the terrain.
class GreebleSource {
public:

    // Represents a sample point in space with pseudorandom values.
    // The GreebleSource must return identical value for a point in space.
    struct Sample {
        // Probability of a greeble detail being added.
        float probability;
        // A small positional offset to reduce appearance of grid sampling.
        glm::vec3 offset;
        // A seed for an RNG.
        uint64_t seed;
    };

public:
    GreebleSource() = default;
    virtual ~GreebleSource() = default;
    virtual int sampleStepSize() const = 0;
    // Return a Sample struct for this point in world space. The Sample struct should be fairly random,
    // but repeated calls to sample() for the same point in space must always return the same value.
    virtual Sample sample(const vec3 world) const = 0;

    // Evaluate a sample, and optionally return a volume sampler to add detail to the Terain.
    // sample: The Sample to evaluate for possibly creating a greeble detail.
    // local: The local coordinate system for the OctreeVolume the greeble detail will be added to.
    // Return an IVolumeSampler to render greeble detail, or null if no detail should be added.
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

    // Sets the index of this TerrainChunk. The origin chunk has an index of (0,0). The
    // next chunk on +x has an index of (1,0), and so on.
    void setIndex(glm::ivec2 index);

    // Get the index, where the "origin" terrain chunk has an index of (0,0)
    glm::ivec2 getIndex() const { return _index; }

    // Returns true of the contents of this TerrainChunk have changed, and it needs to be re-marched.
    bool needsMarch() const { return _needsMarch; }

    /**
     * Kicks off geometry generation, calling onComplete when the geometry has finished generation.
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

/**
 * Maintains a fixed NxN grid of TerrainChunk instances.
 */
class TerrainGrid {
public:
    /**
     * Create a terrain grid composed of gridSize*gridSize TerrainChunks of size chunkSize.
     * Applies the terrainSampleSource evaluator to the grid to produce a continuous terrain function,
     * and applies the greebler evaluator to add detail to the terrain function.
     */
    TerrainGrid(int gridSize, int chunkSize,
        std::unique_ptr<TerrainSampler::SampleSource>&& terrainSampleSource,
        std::unique_ptr<GreebleSource>&& greebler);

    /**
     * Convert a position in world space to the corresponding tile index.
     */
    glm::ivec2 worldToIndex(const glm::vec3& world) const;

    /**
     * Get the TerainChunk which contains the point in world coordinates.
     */
    mc::util::unowned_ptr<TerrainChunk> getTerrainChunkContaining(const glm::vec3& world) const;

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
     * tiles to the left column, assign them the appriate indexes. After calling shift,
     * call march() to regenerate terrain.
     */
    void shift(glm::ivec2 by);

    /**
     * Displays a simple debug listing of TerrainChunks to stdout.
     */
    void print();

    /**
     *Invokes cb() on each TerrainChunk.
     */
    void forEach(std::function<void(mc::util::unowned_ptr<TerrainChunk>)> cb)
    {
        for (const auto& chunk : _grid) {
            cb(chunk.get());
        }
    }

    // Async march all dirty TerrainChunk instances, prioritizing chunks near viewPos,
    // and those viewDir is facing.
    void march(const glm::vec3& viewPos, const glm::vec3& viewDir);

    int getGridSize() const { return _gridSize; }
    glm::vec3 getChunkSize() const { return glm::vec3(_chunkSize); }
    int getCount() const { return _gridSize * _gridSize; }
    bool isMarching() const { return _isMarching; }

    struct RaycastResult {
        static RaycastResult none() { return { false }; }

        bool isHit = false;
        float distance = 0;
        glm::vec3 position { 0 };
        glm::vec3 normal { 0 };

        operator bool() const { return isHit; }
    };

    enum class RaycastEdgeBehavior {
        Zero, // raycast marching outside a terrain segment returns 0
        Clamp // raycast marching outside a terrain segment is clamped to the boundary value.
    };

    /**
     * Perform a raycast against the terrain volume.
     * Raycast from origin, in direction dir. Marches the ray with increments of stepSize, for a max distance of
     * maxLength. If computeNormal is set, will compute the normal of the terrain function (including greebling) at
     * intersection point.
     */
    RaycastResult rayCast(const glm::vec3& origin, const glm::vec3& dir,
        float stepSize, float maxLength, bool computeNormal,
        RaycastEdgeBehavior edgeBehavior = RaycastEdgeBehavior::Clamp) const;

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