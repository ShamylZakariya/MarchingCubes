//
//  marching_cubes.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef marching_cubes_hpp
#define marching_cubes_hpp

#include <functional>

#include "triangle_consumer.hpp"
#include "util/util.hpp"

namespace mc {

typedef std::function<float(const glm::vec3& p)> IsoSurfaceValueFunction;
typedef std::function<glm::vec3(const glm::vec3& p)> IsoSurfaceNormalFunction;

/**
 * The Vertex type generated by the marching cubes algorithm
 */
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

//
// Marching
//

/*
 March region of a volume passing generated triangles into triangleConsumer
 region: The subregion to march
 valueSampler: source of isosurface values
 normalSampler: if provided, will be used to compute per-vertex surface normals. if null,
    each vertex will receive the normal of the triangle it is a part of
 triangleConsumer: Receives each generated triangle
 transform: If not null, a transform to apply to each generated vertex
 */
void march(util::iAABB region,
    IsoSurfaceValueFunction valueSampler,
    IsoSurfaceNormalFunction normalSampler,
    TriangleConsumer<Vertex>& triangleConsumer,
    util::unowned_ptr<glm::mat4> transform);

}

#endif /* marching_cubes_hpp */
