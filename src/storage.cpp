//
//  storage.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "storage.hpp"
#include <iostream>

#pragma mark - Vertex

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
        3,
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

#pragma mark - Vertex Storage

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
        util::CheckGlError("VertexStorage::draw enter");
        glBindVertexArray(_vao);
        glDrawArrays(_mode, 0, static_cast<GLsizei>(_numVertices));
        glBindVertexArray(0);
        util::CheckGlError("VertexStorage::draw exit");
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
    util::CheckGlError("VertexStorage::_updateVertices enter");
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
    util::CheckGlError("VertexStorage::_updateVertices exit");
}
#pragma mark - Indexed Vertex Storage

IndexedVertexStorage::~IndexedVertexStorage()
{
    if (_vao > 0)
        glDeleteVertexArrays(1, &_vao);
    if (_vertexVboId > 0)
        glDeleteBuffers(1, &_vertexVboId);
    if (_indexVboId > 0)
        glDeleteBuffers(1, &_indexVboId);
}

void IndexedVertexStorage::draw() const
{
    if (_vao > 0) {
        util::CheckGlError("IndexedVertexStorage::draw enter");
        glBindVertexArray(_vao);
        glDrawElements(_mode, static_cast<GLsizei>(_numIndices), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        util::CheckGlError("IndexedVertexStorage::draw exit");
    }
}

void IndexedVertexStorage::update(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices)
{
    util::CheckGlError("IndexedVertexStorage::update enter");
    if (_vao == 0) {
        glGenVertexArrays(1, &_vao);
    }
    glBindVertexArray(_vao);
    _updateVertices(vertices);
    _updateIndices(indices);
    glBindVertexArray(0);
    util::CheckGlError("IndexedVertexStorage::update enter");
}

void IndexedVertexStorage::update(const std::vector<Vertex>& vertices)
{
    util::CheckGlError("IndexedVertexStorage::update enter");
    if (_vao == 0) {
        throw std::runtime_error("Can't update vertices without having already assigned indices");
    }
    glBindVertexArray(_vao);
    _updateVertices(vertices);
    glBindVertexArray(0);
    util::CheckGlError("IndexedVertexStorage::update enter");
}

void IndexedVertexStorage::_updateVertices(const std::vector<Vertex>& vertices)
{
    util::CheckGlError("IndexedVertexStorage::_updateVertices enter");
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
    util::CheckGlError("IndexedVertexStorage::_updateVertices exit");
}

void IndexedVertexStorage::_updateIndices(const std::vector<GLuint>& indices)
{
    util::CheckGlError("IndexedVertexStorage::_updateIndices enter");
    if (indices.size() > _indexStorageSize) {
        _indexStorageSize = static_cast<std::size_t>(indices.size() * _growthFactor);
        _numIndices = indices.size();

        if (_indexVboId > 0) {
            glDeleteBuffers(1, &_indexVboId);
            _indexVboId = 0;
        }

        glGenBuffers(1, &_indexVboId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            sizeof(GLuint) * _indexStorageSize,
            nullptr,
            GL_STREAM_DRAW);

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(GLuint) * _numIndices, indices.data());
    } else {
        // GPU storage suffices, just copy data over
        _numIndices = indices.size();
        if (_numIndices > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                0,
                sizeof(GLuint) * _numIndices,
                indices.data());
        }
    }
    util::CheckGlError("IndexedVertexStorage::_updateIndices exit");
}
