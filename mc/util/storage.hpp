//
//  storage.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_storage_h
#define mc_storage_h

#include <epoxy/gl.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>

#include <vector>

namespace mc {
namespace util {

    struct Vertex {
        glm::vec3 pos;
        glm::vec4 color { 1 };
        glm::vec3 normal { 0, 1, 0 };

        enum class AttributeLayout : GLuint {
            Pos = 0,
            Color = 1,
            Normal = 2
        };

        bool operator==(const Vertex& other) const
        {
            return pos == other.pos && color == other.color && normal == other.normal;
        }

        static void bindVertexAttributes();
    };

}
}

namespace std {
template <>
struct hash<mc::util::Vertex> {
    size_t operator()(mc::util::Vertex const& vertex) const
    {
        // https://en.cppreference.com/w/cpp/utility/hash

        std::size_t h0 = hash<glm::vec3>()(vertex.pos);
        std::size_t h1 = hash<glm::vec4>()(vertex.color);
        std::size_t h2 = hash<glm::vec3>()(vertex.normal);

        std::size_t r = (h0 ^ (h1 << 1));
        r = (r ^ (h2 << 1));
        return r;
    }
};
}

namespace mc {
namespace util {

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

}
} // namespace mc::util

#endif /* mc_storage_h */
