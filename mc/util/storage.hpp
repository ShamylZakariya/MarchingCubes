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

    /**
     * Simple vertex type with a 3-component position and 4 component color
     * used by LineSegmentBuffer and skydome rendering
     */
    struct VertexP3C4 {
        glm::vec3 pos;
        glm::vec4 color { 1 };

        enum class AttributeLayout : GLuint {
            Pos = 0,
            Color = 1
        };

        bool operator==(const VertexP3C4& other) const
        {
            return pos == other.pos && color == other.color;
        }

        static void bindVertexAttributes();
    };

    /**
     * GPU storage templated on vertex type. Expects vertex to have static method
     * static void VertexType::bindVertexAttributes(); which sets/enables the right
     * vertex attrib pointers
     */
    template <class VertexType>
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

        VertexStorage(const std::vector<VertexType>& vertices, GLenum mode, float growthFactor = 1.5F)
            : _mode(mode)
            , _growthFactor(growthFactor)
        {
            update(vertices);
        }

        ~VertexStorage()
        {
            if (_vao > 0)
                glDeleteVertexArrays(1, &_vao);
            if (_vertexVboId > 0)
                glDeleteBuffers(1, &_vertexVboId);
        }

        void draw() const
        {
            if (_vao > 0) {
                CHECK_GL_ERROR("VertexStorage::draw enter");
                glBindVertexArray(_vao);
                glDrawArrays(_mode, 0, static_cast<GLsizei>(_numVertices));
                glBindVertexArray(0);
                CHECK_GL_ERROR("VertexStorage::draw exit");
            }
        }

        std::size_t getNumVertices() const { return _numVertices; }
        std::size_t getVertexStoreSize() const { return _vertexStorageSize; }

        void update(const std::vector<VertexType>& vertices)
        {
            if (_vao == 0) {
                glGenVertexArrays(1, &_vao);
            }
            glBindVertexArray(_vao);
            _updateVertices(vertices);
            glBindVertexArray(0);
        }

    private:
        void _updateVertices(const std::vector<VertexType>& vertices)
        {
            CHECK_GL_ERROR("VertexStorage::_updateVertices enter");
            if (vertices.size() > _vertexStorageSize) {
                _vertexStorageSize = static_cast<std::size_t>(vertices.size() * _growthFactor);
                _numVertices = vertices.size();

                if (_vertexVboId > 0) {
                    glDeleteBuffers(1, &_vertexVboId);
                    _vertexVboId = 0;
                }

                glGenBuffers(1, &_vertexVboId);
                glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);

                glBufferData(
                    GL_ARRAY_BUFFER,
                    sizeof(VertexType) * _vertexStorageSize,
                    nullptr,
                    GL_STREAM_DRAW);

                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VertexType) * _numVertices, vertices.data());

                VertexType::bindVertexAttributes();

            } else {
                // GPU storage suffices, just copy data over
                _numVertices = vertices.size();
                if (_numVertices > 0) {
                    glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
                    glBufferSubData(
                        GL_ARRAY_BUFFER,
                        0,
                        sizeof(VertexType) * _numVertices,
                        vertices.data());
                }
            }
            CHECK_GL_ERROR("VertexStorage::_updateVertices exit");
        }
    };

}
} // namespace mc::util

#endif /* mc_storage_h */
