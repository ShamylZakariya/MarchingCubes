//
//  triangle_soup.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "triangle_soup.hpp"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

using namespace glm;
using namespace mc::util;

namespace mc {

//
// TriangleConsumer
//

void TriangleConsumer::start()
{
    _vertices.clear();
    _numTriangles = 0;
}

void TriangleConsumer::addTriangle(const Triangle& t)
{
    _vertices.push_back(t.a);
    _vertices.push_back(t.b);
    _vertices.push_back(t.c);
    _numTriangles++;
}

void TriangleConsumer::finish()
{
    _gpuStorage.update(_vertices);
}

void TriangleConsumer::draw() const
{
    _gpuStorage.draw();
}

} // namespace mc