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

struct TerrainChunk {
private:
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

public:
    /**
     * Create a cube of terrain, where size is the size of an edge of the cube.
     */
    TerrainChunk(int size, FastNoise& noise);

    ~TerrainChunk() = default;
    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk(TerrainChunk&&) = delete;
    TerrainChunk& operator==(const TerrainChunk&) = delete;

    void setIndex(glm::ivec2 index);
    glm::ivec2 getIndex() const { return _index; }

    bool needsMarch() const { return _triangleCount == 0; }

    /**
     * Kicks off geometry generation, calling onComplete when the
     */
    void march(std::function<void()> onComplete);

    /**
     * Returns true if this segment is busy (generating heightmap, or marching the corresponding volume)
     */
    bool isWorking() const { return _isPreparingForMarch || _volume->isMarching(); }

    mc::util::AABB getBounds() const { return _bounds; }
    mc::util::unowned_ptr<mc::OctreeVolume> getVolume() const { return _volume.get(); }
    const std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>>& getGeometry() const { return _triangles; }
    mc::util::LineSegmentBuffer& getAabbLineBuffer() { return _aabbLineBuffer; }
    mc::util::LineSegmentBuffer& getBoundingLineBuffer() { return _boundingLineBuffer; }
    double getLastMarchDurationSeconds() const { return _lastMarchDurationSeconds; }
    int getTriangleCount() const { return _triangleCount; }
    glm::vec3 getWorldOrigin() const { return glm::vec3(_index.x * _size, 0, _index.y * _size); }

private:
    glm::vec2 getXZOffset() const { return _index * _size; }
    void updateHeightmap();

    glm::ivec2 _index;
    int _size = 0;
    mc::util::ThreadPool _threadPool;
    FastNoise& _noise;
    int _triangleCount;
    mc::util::AABB _bounds;
    std::unique_ptr<mc::OctreeVolume> _volume;
    std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>> _triangles;
    mc::util::LineSegmentBuffer _aabbLineBuffer;
    mc::util::LineSegmentBuffer _boundingLineBuffer;
    std::vector<float> _heightmap;
    double _lastMarchDurationSeconds = 0;
    bool _isPreparingForMarch = false;
};

class TerrainGrid {
public:

    /**
     * Create a terrain grid size*size
     */
    TerrainGrid(int gridSize, int chunkSize, FastNoise& noise);

    void shift(glm::ivec2 by);

    void print();

    void forEach(std::function<void(mc::util::unowned_ptr<TerrainChunk>)> cb) {
        for (const auto &chunk : _grid) { cb(chunk.get()); }
    }

    void march(const glm::vec3 &viewPos, const glm::vec3 &viewDir);

    int getGridSize() const { return _gridSize; }
    glm::vec3 getChunkSize() const { return glm::vec3(_chunkSize); }
    int getCount() const { return _gridSize * _gridSize; }

private:
    int _gridSize;
    int _chunkSize;
    std::vector<std::unique_ptr<TerrainChunk>> _grid;
    std::vector<TerrainChunk*> _chunksToMarch;
};

#endif