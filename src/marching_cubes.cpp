//
//  marching_cubes.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "marching_cubes.hpp"
#include "marching_cubes_detail.hpp"

namespace mc {

constexpr float IsoLevel = 0.5F;

//
// IsoSurface
//

vec3 IIsoSurface::normalAt(const vec3& p) const
{
    // from GPUGems 3 Chap 1 -- compute normal of voxel space
    const float d = 0.1f;
    vec3 grad(
        valueAt(p + vec3(d, 0, 0)) - valueAt(p + vec3(-d, 0, 0)),
        valueAt(p + vec3(0, d, 0)) - valueAt(p + vec3(0, -d, 0)),
        valueAt(p + vec3(0, 0, d)) - valueAt(p + vec3(0, 0, -d)));

    return -normalize(grad);
}

//
// March!
//

void march(const IIsoSurface& volume, ITriangleConsumer& tc, const mat4& transform, bool computeNormals)
{
    tc.start();
    march(volume, iAABB(ivec3(0), volume.size()), tc, transform, computeNormals);
    tc.finish();
}

void march(const IIsoSurface& volume, iAABB region, ITriangleConsumer& tc, const mat4& transform, bool computeNormals)
{
    using detail::GetGridCell;
    using detail::GridCell;
    using detail::Polygonise;

    Triangle triangles[5];
    GridCell cell;

    region.min = max(region.min, ivec3(0));
    region.max = min(region.max, volume.size());

    for (int z = region.min.z; z < region.max.z; z++) {
        for (int y = region.min.y; y < region.max.y; y++) {
            for (int x = region.min.x; x < region.max.x; x++) {
                if (GetGridCell(x, y, z, volume, cell, transform)) {
                    for (int t = 0, nTriangles = Polygonise(cell, IsoLevel, volume, triangles, computeNormals); t < nTriangles; t++) {
                        tc.addTriangle(triangles[t]);
                    }
                }
            }
        }
    }
}

//
// ThreadedMarcher
//

ThreadedMarcher::ThreadedMarcher(const IIsoSurface& volume,
    const std::vector<unowned_ptr<ITriangleConsumer>>& tc,
    const mat4& transform,
    bool computeNormals)
    : _volume(volume)
    , _consumers(tc)
    , _transform(transform)
    , _computeNormals(computeNormals)
    , _nThreads(_consumers.size())
    , _slices(_nThreads)
{
    // build thread pool with _nThreads and CPU pinning
    _threads = std::make_unique<ThreadPool>(_nThreads, true);

    // cut _volume into _slices
    auto region = iAABB(ivec3(0), _volume.size());
    auto nThreads = _consumers.size();
    auto sliceSize = static_cast<int>(ceil(static_cast<float>(_volume.size().y) / static_cast<float>(nThreads)));

    for (auto i = 0u; i < nThreads; i++) {
        iAABB slice = region;
        slice.min.y = i * sliceSize;
        slice.max.y = slice.min.y + sliceSize;
        _slices[i] = slice;
    }
}

void ThreadedMarcher::march()
{
    for (auto i = 0u; i < _nThreads; i++) {
        _consumers[i]->start();
        _threads->enqueue([this, i]() {
            mc::march(_volume, _slices[i], *_consumers[i], _transform, _computeNormals);
        });
    }

    _threads->wait();

    for (auto i = 0u; i < _nThreads; i++) {
        _consumers[i]->finish();
    }
}

}
