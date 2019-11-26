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

#pragma mark - IIsoSurface

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

#pragma mark - Marching

void march(const IIsoSurface& surface,
    ITriangleConsumer& tc,
    float isolevel = 0.5f,
    const mat4& transform = mat4(1),
    bool computeNormals = true);

}

#endif /* marching_cubes_hpp */
