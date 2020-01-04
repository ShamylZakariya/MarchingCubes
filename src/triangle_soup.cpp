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
#include <unordered_set>

//
// TriangleConsumer
//

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
    _gpuStorage.update(_vertices);
}

void TriangleConsumer::draw() const
{
    _gpuStorage.draw();
}

//
// IndexedTriangleConsumer
//

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

void IndexedTriangleConsumer::setNormalSmoothingCreaseThresholdRadians(float normalSmoothingCreaseThresholdRadians)
{
    normalSmoothingCreaseThresholdRadians = min(max(normalSmoothingCreaseThresholdRadians, 0.0F), static_cast<float>(M_PI_2));
    _normalSmoothingDotThreshold = cos(normalSmoothingCreaseThresholdRadians);
}

float IndexedTriangleConsumer::normalSmoothingThresholdRadians() const
{
    return acos(_normalSmoothingDotThreshold);
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

    //    std::cout << "IndexedTriangleConsumer::_stitch_Basic raw vertex count: " << _vertices.size() << " unique: " << vertices.size() << " indices: " << indices.size() << std::endl;

    _gpuStorage.update(vertices, indices);
}

namespace normal_smoothing {
// helpers for IndexedTriangleConsumer::_stitch_NormalSmoothing

// a vertex type that cares only about position and color
struct Vertex_PosColor {
    vec3 pos;
    vec3 color;

    bool operator==(const Vertex_PosColor& other) const
    {
        return pos == other.pos && color == other.color;
    }
};

/*
Looks for best match by normal for v to be merged with one of the vertices referred to by search indices; if none is a good match, adds vertex to vertices. Returns the index to use.
 */
GLuint addVertex(std::vector<Vertex>& vertices,
    std::vector<uint32_t>& searchIndices,
    std::vector<uint32_t>& globalIndices,
    std::unordered_set<GLuint>& indicesToNormalize,
    Vertex v,
    float dotThreshold)
{
    Vertex* bestMatch = nullptr;
    float bestMatchValue = -1;
    GLuint bestMatchIdx = 0;
    for (auto searchIndex : searchIndices) {
        Vertex* refVertex = &(vertices[searchIndex]);
        vec3 refVertexNormal = normalize(refVertex->normal);
        float d = dot(refVertexNormal, v.normal);
        if (d >= dotThreshold && d > bestMatchValue) {
            bestMatchValue = d;
            bestMatch = refVertex;
            bestMatchIdx = searchIndex;
        }
    }

    if (bestMatch != nullptr) {
        bestMatch->normal += v.normal;
        indicesToNormalize.insert(bestMatchIdx);

        return bestMatchIdx;
    } else {
        uint32_t idx = static_cast<GLuint>(vertices.size());
        vertices.push_back(v);
        searchIndices.push_back(idx);
        globalIndices.push_back(idx);

        return idx;
    }
}

}

namespace std {
template <>
struct hash<normal_smoothing::Vertex_PosColor> {
    size_t operator()(normal_smoothing::Vertex_PosColor const& vertex) const
    {
        return (hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1;
    }
};
}

void IndexedTriangleConsumer::_stitch_NormalSmoothing()
{
    using normal_smoothing::addVertex;
    using normal_smoothing::Vertex_PosColor;

    std::unordered_map<Vertex_PosColor, std::vector<uint32_t>> uniqueVertices = {};
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::unordered_set<GLuint> indicesToNormalize;

    for (const auto& v : _vertices) {
        Vertex_PosColor id { v.pos, v.color };
        auto pos = uniqueVertices.find(id);
        if (pos == std::end(uniqueVertices)) {
            // this is a new pos/color pairing
            auto idx = static_cast<GLuint>(vertices.size());
            vertices.push_back(v);
            uniqueVertices[id].push_back(idx);
            indices.push_back(idx);
        } else {
            // we have this pos/color pair, so we need to perform a search; if
            // we already have a vertex with close enough normal we add our normal
            // to it (and re-normalize later) otherwise we add a new vertex
            auto idx = addVertex(vertices, pos->second, indices, indicesToNormalize, v, _normalSmoothingDotThreshold);
            indices.push_back(idx);
        }
    }

    // re-normalize
    for (auto idx : indicesToNormalize) {
        vertices[idx].normal = normalize(vertices[idx].normal);
    }

    //    std::cout << "IndexedTriangleConsumer::_stitch_NormalSmoothing raw vertex count: " << _vertices.size() << " unique: " << vertices.size() << " indices: " << indices.size() << std::endl;

    _gpuStorage.update(vertices, indices);
}
