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

    const float kFloorThreshold = 1.0F;

public:
    TerrainChunk(int size, std::shared_ptr<mc::util::ThreadPool> threadPool, FastNoise& noise);

    ~TerrainChunk() = default;
    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk(TerrainChunk&&) = delete;
    TerrainChunk& operator==(const TerrainChunk&) = delete;

    void build(int idx);

    void march();

    /**
     * Returns true if this segment is busy (generating heightmap, or marching the corresponding volume)
     */
    bool isWorking() const { return _isUpdatingHeightmap || _volume->isMarching(); }

    int getIdx() const { return _idx; }
    mc::util::unowned_ptr<mc::OctreeVolume> getVolume() const { return _volume.get(); }
    const std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>>& getGeometry() const { return _triangles; }
    mc::util::LineSegmentBuffer& getAabbLineBuffer() { return _aabbLineBuffer; }
    mc::util::LineSegmentBuffer& getBoundingLineBuffer() { return _boundingLineBuffer; }
    double getLastMarchDurationSeconds() const { return _lastMarchDurationSeconds; }
    int getTriangleCount() const { return _triangleCount; }

private:
    void updateHeightmap();
    glm::vec4 nodeColor(int atDepth);

    int _idx = 0;
    int _size = 0;
    std::shared_ptr<mc::util::ThreadPool> _threadPool;
    FastNoise& _noise;
    int _triangleCount;
    std::unique_ptr<mc::OctreeVolume> _volume;
    std::vector<std::unique_ptr<mc::TriangleConsumer<mc::Vertex>>> _triangles;
    mc::util::LineSegmentBuffer _aabbLineBuffer;
    mc::util::LineSegmentBuffer _boundingLineBuffer;
    std::vector<float> _heightmap;
    double _lastMarchDurationSeconds = 0;
    bool _isUpdatingHeightmap = false;
    std::vector<glm::vec4> _nodeColors;
};

#endif