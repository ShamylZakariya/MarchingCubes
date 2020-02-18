//
//  storage.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include <iostream>

#include "io.hpp"
#include "storage.hpp"

using namespace glm;

namespace mc {
namespace util {

    //
    // Vertex
    //

    void Vertex::bindVertexAttributes()
    {
        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Pos),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Pos));

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Color),
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, color));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Color));

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Normal),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Normal));
    }

    //
    // Vertex Storage
    //

    VertexStorage::~VertexStorage()
    {
        if (_vao > 0)
            glDeleteVertexArrays(1, &_vao);
        if (_vertexVboId > 0)
            glDeleteBuffers(1, &_vertexVboId);
    }

    void VertexStorage::draw() const
    {
        if (_vao > 0) {
            CheckGlError("VertexStorage::draw enter");
            glBindVertexArray(_vao);
            glDrawArrays(_mode, 0, static_cast<GLsizei>(_numVertices));
            glBindVertexArray(0);
            CheckGlError("VertexStorage::draw exit");
        }
    }

    void VertexStorage::update(const std::vector<Vertex>& vertices)
    {
        if (_vao == 0) {
            glGenVertexArrays(1, &_vao);
        }
        glBindVertexArray(_vao);
        _updateVertices(vertices);
        glBindVertexArray(0);
    }

    void VertexStorage::_updateVertices(const std::vector<Vertex>& vertices)
    {
        CheckGlError("VertexStorage::_updateVertices enter");
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
                sizeof(Vertex) * _vertexStorageSize,
                nullptr,
                GL_STREAM_DRAW);

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * _numVertices, vertices.data());

            Vertex::bindVertexAttributes();

        } else {
            // GPU storage suffices, just copy data over
            _numVertices = vertices.size();
            if (_numVertices > 0) {
                glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
                glBufferSubData(
                    GL_ARRAY_BUFFER,
                    0,
                    sizeof(Vertex) * _numVertices,
                    vertices.data());
            }
        }
        CheckGlError("VertexStorage::_updateVertices exit");
    }

}
} // namespace mc::util