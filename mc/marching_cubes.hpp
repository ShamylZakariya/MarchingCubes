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
 normalSampler: if computeNormals is true, this will be called to sample the normals at points
 triangleConsumer: Receives each generated triangle
 transform: A transform to apply to each generated vertex
 computeNormals: If true, vertex normals will be computed via IIsoSurface::normalAt; else, generated triangle normals will be used
 TODO: Make a variant which doesn't take transform and is faster
 */
void march(util::iAABB region, IsoSurfaceValueFunction valueSampler, IsoSurfaceNormalFunction normalSampler, ITriangleConsumer& tc, const glm::mat4& transform, bool computeNormals);

}

#endif /* marching_cubes_hpp */
