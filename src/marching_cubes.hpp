//
//  marching_cubes.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef marching_cubes_hpp
#define marching_cubes_hpp

#include "aabb.hpp"
#include "triangle_soup.hpp"
#include "util.hpp"

namespace mc {

//
// IIsoSurface
//

class IIsoSurface {
public:
    IIsoSurface() = default;
    virtual ~IIsoSurface() = default;

    /*
     Return the bounded size of the isosurface volume
     */
    virtual ivec3 size() const = 0;

    /*
     Return the value of the isosurface at a point in space, where 0 means "nothing there" and 1 means fully occupied.
     */
    virtual float valueAt(const vec3& p) const = 0;

    /*
     Return the normal of the gradient at a point in space.
     Default implementation takes 6 taps about the sample point to compute local gradient.
     Custom IsoSurface implementations may be able to implement a smarter domain-specific approach.
     */
    virtual vec3 normalAt(const vec3& p) const;
};

//
// Marching
//

/*
 March entire volume passing generated triangles into triangleConsumer
 volume: The volume to march
 triangleConsumer: Receives each generated triangle
 transform: A transform to apply to each generated vertex
 computeNormals: If true, vertex normals will be computed via IIsoSurface::normalAt; else, generated triangle normals will be used
 */
void march(const IIsoSurface& volume,
    ITriangleConsumer& triangleConsumer,
    const mat4& transform = mat4(1),
    bool computeNormals = true);

/*
 March subregion of a volume passing generated triangles into triangleConsumer
 volume: The volume to march
 region: The subregion to march
 triangleConsumer: Receives each generated triangle
 transform: A transform to apply to each generated vertex
 computeNormals: If true, vertex normals will be computed via IIsoSurface::normalAt; else, generated triangle normals will be used
 */
void march(const mc::IIsoSurface& volume, iAABB region, ITriangleConsumer& tc, const mat4& transform = mat4(1), bool computeNormals = true);

/*
 Marches a volume using a thread pool where each thread processes one "slice" of the volume.
 volume: The volume to march
 triangleConsumers: Each consumer receives a slice of the marched volume
 transform: A transform to apply to each generated vertex
 computeNormals: If true, vertex normals will be computed via IIsoSurface::normalAt; else, generated triangle normals will be used
 */
class ThreadedMarcher {
public:
    ThreadedMarcher(const IIsoSurface& volume,
        const std::vector<unowned_ptr<ITriangleConsumer>>& triangleConsumers,
        const mat4& transform = mat4(1),
        bool computeNormals = true);

    ~ThreadedMarcher() = default;

    /*
     March the current state of the volume; the triangle consumers
     which were passed during construction will be updated with new
     geometry.
     Note: ITriangleConsumer::start() and ITriangleConsumer::finish()
     will be called on the calling thread; ITriangleConsumer::addTriangle()
     will be called via a dedicated thread owned by the internal pool.
     */
    void march();

private:
    const IIsoSurface& _volume;
    std::vector<unowned_ptr<ITriangleConsumer>> _consumers;
    std::unique_ptr<ThreadPool> _threads;
    mat4 _transform;
    bool _computeNormals;
    std::size_t _nThreads;
    std::vector<iAABB> _slices;
};

}

#endif /* marching_cubes_hpp */
