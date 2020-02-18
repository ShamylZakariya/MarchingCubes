//
//  triangle_soup.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef triangle_soup_hpp
#define triangle_soup_hpp

#include "util/util.hpp"

namespace mc {

template <class VertexType>
struct Triangle {
    VertexType a, b, c;

    Triangle()
    {
    }

    Triangle(const VertexType& a, const VertexType& b, const VertexType& c)
        : a(a)
        , b(b)
        , c(c)
    {
    }

    Triangle(const Triangle<VertexType>& other)
    {
        a = other.a;
        b = other.b;
        c = other.c;
    }

    Triangle& operator=(const Triangle<VertexType>& other)
    {
        a = other.a;
        b = other.b;
        c = other.c;
        return *this;
    }
};

/*
 Consumes triangles and maintaining a non-indexed vertex storage
 */
template <class VertexType>
class TriangleConsumer {
private:
    std::vector<VertexType> _vertices;
    util::VertexStorage<VertexType> _gpuStorage { GL_TRIANGLES };
    size_t _numTriangles = 0;

public:
    TriangleConsumer() = default;
    virtual ~TriangleConsumer() = default;

    void start()
    {
        _vertices.clear();
        _numTriangles = 0;
    }

    void addTriangle(const Triangle<VertexType>& t)
    {
        _vertices.push_back(t.a);
        _vertices.push_back(t.b);
        _vertices.push_back(t.c);
        _numTriangles++;
    }

    size_t numTriangles() const { return _numTriangles; }
    void finish()
    {
        _gpuStorage.update(_vertices);
    }

    void draw() const
    {
        _gpuStorage.draw();
    }

    const auto& storage() const { return _gpuStorage; }
    auto& storage() { return _gpuStorage; }
};

} // namespace mc

#endif /* triangle_soup_hpp */
