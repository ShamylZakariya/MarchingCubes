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

constexpr float IsoLevel = 0.5F;

void march(iAABB region,
    IsoSurfaceValueFunction valueSampler,
    IsoSurfaceNormalFunction normalSampler,
    ITriangleConsumer& tc,
    const mat4& transform,
    bool computeNormals)
{
    Triangle triangles[5];
    detail::GridCell cell;

    for (int z = region.min.z; z < region.max.z; z++) {
        for (int y = region.min.y; y < region.max.y; y++) {
            for (int x = region.min.x; x < region.max.x; x++) {
                if (detail::GetGridCell(x, y, z, valueSampler, cell, transform)) {
                    for (int t = 0, nTriangles = detail::Polygonise(cell, IsoLevel, normalSampler, triangles, computeNormals); t < nTriangles; t++) {
                        tc.addTriangle(triangles[t]);
                    }
                }
            }
        }
    }
}

}
