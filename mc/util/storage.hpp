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

}
} // namespace mc::util

#endif /* mc_storage_h */
