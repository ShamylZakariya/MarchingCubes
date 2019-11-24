//
//  storage.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef storage_hpp
#define storage_hpp

#include "util.hpp"

#include <vector>

struct Vertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec3 barycentric;

    enum class AttributeLayout : GLuint {
        Pos = 0,
        Color = 1,
        Normal = 2,
        Barycentric = 3
    };

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos &&
            color == other.color &&
            normal == other.normal &&
            barycentric == other.barycentric;
    }

    static void bindVertexAttributes();
};

namespace std {
template <>
struct hash<Vertex> {
    size_t operator()(Vertex const& vertex) const
    {
        // https://en.cppreference.com/w/cpp/utility/hash

        std::size_t h0 = hash<glm::vec3>()(vertex.pos);
        std::size_t h1 = hash<glm::vec3>()(vertex.color);
        std::size_t h2 = hash<glm::vec3>()(vertex.normal);
        std::size_t h3 = hash<glm::vec3>()(vertex.barycentric);
        
        std::size_t r = (h0 ^ (h1 << 1));
        r = (r ^ (h2 << 1));
        r = (r ^ (h3 << 1));
        return r;
    }
};
}

class VertexStorage {
private:
    GLenum _mode;
    GLuint _vertexVboId = 0;
    GLuint _vao = 0;
    std::size_t _numVertices = 0;
    std::size_t _vertexStorageSize = 0;
    float _growthFactor;

public:
    VertexStorage(GLenum mode, float growthFactor = 1.5F)
        : _mode(mode)
        , _growthFactor(growthFactor)
    {
    }

    VertexStorage(const std::vector<Vertex>& vertices, GLenum mode, float growthFactor = 1.5F)
        : _mode(mode)
        , _growthFactor(growthFactor)
    {
        update(vertices);
    }

    ~VertexStorage();

    void draw() const;

    std::size_t numVertices() const { return _numVertices; }
    std::size_t vertexStoreSize() const { return _vertexStorageSize; }

    void update(const std::vector<Vertex>& vertices);

private:
    void _updateVertices(const std::vector<Vertex>& vertices);
};

class IndexedVertexStorage {
private:
    GLenum _mode;
    GLuint _vertexVboId = 0;
    GLuint _indexVboId = 0;
    GLuint _vao = 0;
    std::size_t _numIndices = 0;
    std::size_t _numVertices = 0;
    std::size_t _vertexStorageSize = 0;
    std::size_t _indexStorageSize = 0;
    float _growthFactor;

public:
    IndexedVertexStorage(GLenum mode = GL_TRIANGLES, float growthFactor = 1.5F)
        : _mode(mode)
        , _growthFactor(growthFactor)
    {
    }

    IndexedVertexStorage(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices, GLenum mode = GL_TRIANGLES, float growthFactor = 1.5F)
        : _mode(mode)
        , _growthFactor(growthFactor)
    {
        update(vertices, indices);
    }

    ~IndexedVertexStorage();

    void draw() const;

    std::size_t numVertices() const { return _numVertices; }
    std::size_t vertexStoreSize() const { return _vertexStorageSize; }
    std::size_t numIndices() const { return _numIndices; }
    std::size_t indexStoreSize() const { return _indexStorageSize; }

    void update(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices);
    void update(const std::vector<Vertex>& vertices);

private:
    void _updateVertices(const std::vector<Vertex>& vertices);
    void _updateIndices(const std::vector<GLuint>& indices);
};

#endif /* storage_hpp */
