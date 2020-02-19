//
//  marching_cubes.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "marching_cubes.hpp"
#include "marching_cubes_detail.hpp"

using namespace glm;
using namespace mc::util;

namespace mc {

namespace {
    constexpr float IsoLevel = 0.5F;
}

void Vertex::bindVertexAttributes()
{
    glVertexAttribPointer(
        static_cast<GLuint>(AttributeLayout::Pos),
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Pos));

    glVertexAttribPointer(
        static_cast<GLuint>(AttributeLayout::Color),
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, color));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Color));

    glVertexAttribPointer(
        static_cast<GLuint>(Vertex::AttributeLayout::VertexNormal),
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, vertexNormal));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::VertexNormal));

    glVertexAttribPointer(
        static_cast<GLuint>(Vertex::AttributeLayout::TriangleNormal),
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, triangleNormal));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::TriangleNormal));
}

void march(iAABB region,
    IsoSurfaceValueFunction valueSampler,
    IsoSurfaceNormalFunction normalSampler,
    TriangleConsumer<Vertex>& tc,
    const util::unowned_ptr<glm::mat4> transform)
{
    Triangle<Vertex> triangles[5];
    detail::GridCell cell;

    for (int z = region.min.z; z < region.max.z; z++) {
        for (int y = region.min.y; y < region.max.y; y++) {
            for (int x = region.min.x; x < region.max.x; x++) {
                if (detail::GetGridCell(x, y, z, valueSampler, cell, transform)) {
                    for (int t = 0, nTriangles = detail::Polygonise(cell, IsoLevel, normalSampler, triangles); t < nTriangles; t++) {
                        tc.addTriangle(triangles[t]);
                    }
                }
            }
        }
    }
}

}
