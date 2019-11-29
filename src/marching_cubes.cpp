//
//  marching_cubes.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include <thread>

#include "marching_cubes.hpp"
#include "marching_cubes_detail.hpp"

#pragma mark - IsoSurface

vec3 mc::IIsoSurface::normalAt(const vec3& p) const
{
    // from GPUGems 3 Chap 1 -- compute normal of voxel space
    const float d = 0.1f;
    vec3 grad(
        valueAt(p + vec3(d, 0, 0)) - valueAt(p + vec3(-d, 0, 0)),
        valueAt(p + vec3(0, d, 0)) - valueAt(p + vec3(0, -d, 0)),
        valueAt(p + vec3(0, 0, d)) - valueAt(p + vec3(0, 0, -d)));

    return -normalize(grad);
}

#pragma mark - March!

void mc::march(const mc::IIsoSurface& volume, ITriangleConsumer& tc, float isolevel, const mat4& transform, bool computeNormals)
{
    tc.start();
    march(volume, AABBi(ivec3(0), volume.size()), tc, isolevel, transform, computeNormals);
    tc.finish();
}

void mc::march(const mc::IIsoSurface& volume, AABBi region, ITriangleConsumer& tc, float isolevel, const mat4& transform, bool computeNormals)
{
    using mc::detail::GetGridCell;
    using mc::detail::GridCell;
    using mc::detail::Polygonise;

    Triangle triangles[5];
    GridCell cell;

    region.min = max(region.min, ivec3(0));
    region.max = min(region.max, volume.size());

    for (int z = region.min.z; z < region.max.z; z++) {
        for (int y = region.min.y; y < region.max.y; y++) {
            for (int x = region.min.x; x < region.max.x; x++) {
                if (GetGridCell(x, y, z, volume, cell, transform)) {
                    for (int t = 0, nTriangles = Polygonise(cell, isolevel, volume, triangles, computeNormals); t < nTriangles; t++) {
                        tc.addTriangle(triangles[t]);
                    }
                }
            }
        }
    }
}

void mc::march_multithreaded(const IIsoSurface& volume,
    const std::vector<unowned_ptr<ITriangleConsumer>>& tc,
    float isoLevel,
    const mat4& transform,
    bool computeNormals)
{
    auto region = AABBi(ivec3(0), volume.size());
    auto nThreads = tc.size();
    
    // split volume into nThreads slices along Y axis
    int sliceSize = static_cast<int>(ceil(static_cast<float>(volume.size().y) / static_cast<float>(nThreads)));
    
    // start threads to render slices
    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    for (auto i = 0; i < nThreads; i++) {
        tc[i]->start();
        threads.emplace_back([i, sliceSize,&volume,&tc,region, isoLevel, &transform, computeNormals](){
            AABBi slice = region;
            slice.min.y = i * sliceSize;
            slice.max.y = slice.min.y + sliceSize;
            march(volume, slice, *tc[i], isoLevel, transform, computeNormals);
        });
    }
    
    for (auto i = 0; i < nThreads; i++) {
        threads[i].join();
        tc[i]->finish();
    }    
}
