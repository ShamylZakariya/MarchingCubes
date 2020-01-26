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

#include "triangle_soup.hpp"
#include "util/util.hpp"

namespace mc {

typedef std::function<float(const glm::vec3& p)> IsoSurfaceValueFunction;
typedef std::function<glm::vec3(const glm::vec3& p)> IsoSurfaceNormalFunction;

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
    ITriangleConsumer& triangleConsumer,
    util::unowned_ptr<glm::mat4> transform);

}

#endif /* marching_cubes_hpp */
