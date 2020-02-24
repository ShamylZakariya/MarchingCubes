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

    /*
    From "Polygonising A Scalar Field" by Paul Bourke
    http://paulbourke.net/geometry/polygonise/
    */

    //
    // GridCell
    //

    class GridCell {
    public:
        glm::vec3 pos[8];
        float val[8];
        MaterialState material[8];
        bool occupied;

        GridCell()
            : occupied(false)
        {
            for (int i = 0; i < 8; i++)
                val[i] = 0;
        }

        GridCell(const GridCell& c) = delete;
        GridCell& operator=(const GridCell& c) = delete;
    };

    //
    // MarchingCubes Implementation
    //

    /*
    Linearly interpolate the position where an isosurface cuts
    an edge between two vertices, each with their own scalar value
    */

    glm::vec3 IsoStateInterpolator(float isolevel, const glm::vec3& p1, const glm::vec3& p2, float valp1, float valp2)
    {
        constexpr float EPSILON = 1e-5f;
        if (std::abs(isolevel - valp1) < EPSILON)
            return p1;

        if (std::abs(isolevel - valp2) < EPSILON)
            return p2;

        if (std::abs(valp1 - valp2) < EPSILON)
            return p1;

        auto mu = (isolevel - valp1) / (valp2 - valp1);
        return mix(p1, p2, mu);
    }

    /*
    Given a grid cell and an isolevel, calculate the triangular
    facets required to represent the isosurface through the cell.
    Return the number of triangular facets, the array "triangles"
    will be loaded up with the vertices at most 5 triangular facets.
    0 will be returned if the grid cell is either totally above
    of totally below the isolevel.
    */

    int Polygonise(const GridCell& grid, float isolevel, Triangle<Vertex>* triangles)
    {
        /*
        Determine the index into the edge table which
        tells us which vertices are inside of the surface
     */
        int cubeIndex = 0;
        if (grid.val[0] < isolevel)
            cubeIndex |= 1;
        if (grid.val[1] < isolevel)
            cubeIndex |= 2;
        if (grid.val[2] < isolevel)
            cubeIndex |= 4;
        if (grid.val[3] < isolevel)
            cubeIndex |= 8;
        if (grid.val[4] < isolevel)
            cubeIndex |= 16;
        if (grid.val[5] < isolevel)
            cubeIndex |= 32;
        if (grid.val[6] < isolevel)
            cubeIndex |= 64;
        if (grid.val[7] < isolevel)
            cubeIndex |= 128;

        /*
        Cube is entirely in/out of the surface
    */
        if (detail::kEdgeTable[cubeIndex] == 0)
            return 0;

        /*
        Find the vertices where the surface intersects the cube
    */
        glm::vec3 vertList[12];

        if (detail::kEdgeTable[cubeIndex] & 1) {
            vertList[0] = IsoStateInterpolator(isolevel, grid.pos[0], grid.pos[1], grid.val[0], grid.val[1]);
        }

        if (detail::kEdgeTable[cubeIndex] & 2) {
            vertList[1] = IsoStateInterpolator(isolevel, grid.pos[1], grid.pos[2], grid.val[1], grid.val[2]);
        }

        if (detail::kEdgeTable[cubeIndex] & 4) {
            vertList[2] = IsoStateInterpolator(isolevel, grid.pos[2], grid.pos[3], grid.val[2], grid.val[3]);
        }

        if (detail::kEdgeTable[cubeIndex] & 8) {
            vertList[3] = IsoStateInterpolator(isolevel, grid.pos[3], grid.pos[0], grid.val[3], grid.val[0]);
        }

        if (detail::kEdgeTable[cubeIndex] & 16) {
            vertList[4] = IsoStateInterpolator(isolevel, grid.pos[4], grid.pos[5], grid.val[4], grid.val[5]);
        }

        if (detail::kEdgeTable[cubeIndex] & 32) {
            vertList[5] = IsoStateInterpolator(isolevel, grid.pos[5], grid.pos[6], grid.val[5], grid.val[6]);
        }

        if (detail::kEdgeTable[cubeIndex] & 64) {
            vertList[6] = IsoStateInterpolator(isolevel, grid.pos[6], grid.pos[7], grid.val[6], grid.val[7]);
        }

        if (detail::kEdgeTable[cubeIndex] & 128) {
            vertList[7] = IsoStateInterpolator(isolevel, grid.pos[7], grid.pos[4], grid.val[7], grid.val[4]);
        }

        if (detail::kEdgeTable[cubeIndex] & 256) {
            vertList[8] = IsoStateInterpolator(isolevel, grid.pos[0], grid.pos[4], grid.val[0], grid.val[4]);
        }

        if (detail::kEdgeTable[cubeIndex] & 512) {
            vertList[9] = IsoStateInterpolator(isolevel, grid.pos[1], grid.pos[5], grid.val[1], grid.val[5]);
        }

        if (detail::kEdgeTable[cubeIndex] & 1024) {
            vertList[10] = IsoStateInterpolator(isolevel, grid.pos[2], grid.pos[6], grid.val[2], grid.val[6]);
        }

        if (detail::kEdgeTable[cubeIndex] & 2048) {
            vertList[11] = IsoStateInterpolator(isolevel, grid.pos[3], grid.pos[7], grid.val[3], grid.val[7]);
        }

        //
        //    Create the triangle
        //

        int numTriangles = 0;
        for (int i = 0; detail::kTriTable[cubeIndex][i] != -1; i += 3) {
            triangles[numTriangles].a.pos = vertList[detail::kTriTable[cubeIndex][i]];
            triangles[numTriangles].b.pos = vertList[detail::kTriTable[cubeIndex][i + 1]];
            triangles[numTriangles].c.pos = vertList[detail::kTriTable[cubeIndex][i + 2]];

            glm::vec3 n = normalize(cross(triangles[numTriangles].b.pos - triangles[numTriangles].a.pos, triangles[numTriangles].c.pos - triangles[numTriangles].a.pos));
            triangles[numTriangles].a.triangleNormal = n;
            triangles[numTriangles].b.triangleNormal = n;
            triangles[numTriangles].c.triangleNormal = n;
            numTriangles++;
        }

        return numTriangles;
    }

    //
    // GridCell Access
    //

    bool GetGridCell(int x, int y, int z, IsoSurfaceValueFunction valueFunction, GridCell& cell)
    {
        // store the location in the voxel array
        cell.pos[0] = glm::vec3(x, y, z);
        cell.pos[1] = glm::vec3(x + 1, y, z);
        cell.pos[2] = glm::vec3(x + 1, y + 1, z);
        cell.pos[3] = glm::vec3(x, y + 1, z);

        cell.pos[4] = glm::vec3(x, y, z + 1);
        cell.pos[5] = glm::vec3(x + 1, y, z + 1);
        cell.pos[6] = glm::vec3(x + 1, y + 1, z + 1);
        cell.pos[7] = glm::vec3(x, y + 1, z + 1);

        // store the value in the voxel array
        cell.val[0] = valueFunction(cell.pos[0], cell.material[0]);
        cell.val[1] = valueFunction(cell.pos[1], cell.material[1]);
        cell.val[2] = valueFunction(cell.pos[2], cell.material[2]);
        cell.val[3] = valueFunction(cell.pos[3], cell.material[3]);

        cell.val[4] = valueFunction(cell.pos[4], cell.material[4]);
        cell.val[5] = valueFunction(cell.pos[5], cell.material[5]);
        cell.val[6] = valueFunction(cell.pos[6], cell.material[6]);
        cell.val[7] = valueFunction(cell.pos[7], cell.material[7]);

        cell.occupied = (cell.val[0] > 0
            || cell.val[1] > 0
            || cell.val[2] > 0
            || cell.val[3] > 0
            || cell.val[4] > 0
            || cell.val[5] > 0
            || cell.val[6] > 0
            || cell.val[7] > 0);

        return cell.occupied;
    }

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
        static_cast<GLuint>(Vertex::AttributeLayout::TriangleNormal),
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, triangleNormal));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::TriangleNormal));

    glVertexAttribPointer(
        static_cast<GLuint>(Vertex::AttributeLayout::Shininess),
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, shininess));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Shininess));

    glVertexAttribPointer(
        static_cast<GLuint>(Vertex::AttributeLayout::Texture0),
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, texture0));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Texture0));

    glVertexAttribPointer(
        static_cast<GLuint>(Vertex::AttributeLayout::Texture1),
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (const GLvoid*)offsetof(Vertex, texture1));
    glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Texture1));

}

void march(iAABB region,
    IsoSurfaceValueFunction valueSampler,
    TriangleConsumer<Vertex>& tc)
{
    Triangle<Vertex> triangles[5];
    GridCell cell;
    constexpr float IsoLevel = 0.5F;

    for (int z = region.min.z; z < region.max.z; z++) {
        for (int y = region.min.y; y < region.max.y; y++) {
            for (int x = region.min.x; x < region.max.x; x++) {
                if (GetGridCell(x, y, z, valueSampler, cell)) {
                    for (int t = 0, nTriangles = Polygonise(cell, IsoLevel, triangles); t < nTriangles; t++) {
                        tc.addTriangle(triangles[t]);
                    }
                }
            }
        }
    }
}

}
