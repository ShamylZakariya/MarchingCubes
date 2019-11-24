//
//  triangle_soup.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "triangle_soup.hpp"

#include <iostream>
#include <unordered_map>

#pragma mark - TriangleConsumer

void TriangleConsumer::start()
{
    _vertices.clear();
}

void TriangleConsumer::addTriangle(const Triangle& t)
{
    _vertices.push_back(t.a);
    _vertices.push_back(t.b);
    _vertices.push_back(t.c);
}

void TriangleConsumer::finish()
{
    auto count = _gpuStorage.vertexStoreSize();
    _gpuStorage.update(_vertices);
    auto newCount = _gpuStorage.vertexStoreSize();
    if (newCount != count) {
        std::cout << "TriangleConsumer::finish() - VertexStorage increased storage from " << count << " to " << newCount << " vertices" << std::endl;
    }
}

void TriangleConsumer::draw() const
{
    _gpuStorage.draw();
}

#pragma mark - IndexedTriangleConsumer

void IndexedTriangleConsumer::start()
{
    _vertices.clear();
}

void IndexedTriangleConsumer::addTriangle(const Triangle& t)
{
    _vertices.push_back(t.a);
    _vertices.push_back(t.b);
    _vertices.push_back(t.c);
}

void IndexedTriangleConsumer::finish()
{
    switch (_strategy) {
    case Strategy::Basic:
        _stitch_Basic();
        break;
    case Strategy::NormalSmoothing:
        _stitch_NormalSmoothing();
        break;
    }
}

void IndexedTriangleConsumer::draw() const
{
    _gpuStorage.draw();
}

void IndexedTriangleConsumer::_stitch_Basic()
{
    std::unordered_map<Vertex, uint32_t> uniqueVertices = {};
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    for (const auto& v : _vertices) {
        auto pos = uniqueVertices.find(v);
        if (pos == std::end(uniqueVertices)) {
            auto idx = static_cast<GLuint>(vertices.size());
            uniqueVertices[v] = idx;
            vertices.push_back(v);
            indices.push_back(idx);
        } else {
            indices.push_back(pos->second);
        }
    }

    std::cout << "IndexedTriangleConsumer::_stitch_Basic raw vertex count: " << _vertices.size() << " unique: " << vertices.size() << " indices: " << indices.size() << std::endl;

    auto vStorage = _gpuStorage.vertexStoreSize();
    auto iStorage = _gpuStorage.indexStoreSize();
    _gpuStorage.update(vertices, indices);
    auto vNewStorage = _gpuStorage.vertexStoreSize();
    auto iNewStorage = _gpuStorage.indexStoreSize();
    if (vStorage != vNewStorage || iStorage != iNewStorage) {
        std::cout << "IndexedTriangleConsumer::_stitch_Basic vertexStorage went from " << vStorage << " to " << vNewStorage << " and index storage went from " << iStorage << " to " << iNewStorage << std::endl;
    }
}

void IndexedTriangleConsumer::_stitch_NormalSmoothing()
{
    throw std::runtime_error("IndexedTriangleConsumer::_stitch_NormalSmoothing - Unimplemented");
}
