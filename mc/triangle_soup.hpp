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

struct Triangle {
    util::Vertex a, b, c;

    Triangle()
    {
    }

    Triangle(const util::Vertex& a, const util::Vertex& b, const util::Vertex& c)
        : a(a)
        , b(b)
        , c(c)
    {
    }

    Triangle(const Triangle& other)
    {
        a = other.a;
        b = other.b;
        c = other.c;
    }

    Triangle& operator=(const Triangle& other)
    {
        a = other.a;
        b = other.b;
        c = other.c;
        return *this;
    }
};

class ITriangleConsumer {
public:
    ITriangleConsumer() = default;
    virtual ~ITriangleConsumer() = default;

    /*
     Signal that a batch of triangles will begin. Clears internal storage.
     */
    virtual void start() = 0;

    /*
     Add a triangle to storage
     */
    virtual void addTriangle(const Triangle& t) = 0;

    /*
    Count of triangles added since last call to start()
    */
    virtual size_t numTriangles() const = 0;

    /*
     Signal that batch addition has completed; submits new triangles to GPU
     */
    virtual void finish() = 0;

    /*
     Draw the internal storage of triangles
     */
    virtual void draw() const = 0;
};

/*
 TriangleConsumer which does nothing but record how many triangles were delivered between start() and end();
 meant for profiling, testing, etc.
 */
class CountingTriangleConsumer : public ITriangleConsumer {
public:
    CountingTriangleConsumer() = default;

    void start() override
    {
        _triangleCount = 0;
    }

    void addTriangle(const Triangle& t) override
    {
        _triangleCount++;
    }

    size_t numTriangles() const override { return _triangleCount; }

    void finish() override
    {
    }

    void draw() const override {}

private:
    size_t _triangleCount = 0;
};

/*
 Consumes triangles and maintaining a non-indexed vertex storage
 */
class TriangleConsumer : public ITriangleConsumer {
private:
    std::vector<util::Vertex> _vertices;
    util::VertexStorage _gpuStorage { GL_TRIANGLES };
    size_t _numTriangles = 0;

public:
    TriangleConsumer() = default;
    virtual ~TriangleConsumer() = default;

    void start() override;
    void addTriangle(const Triangle& t) override;
    size_t numTriangles() const override { return _numTriangles; }
    void finish() override;
    void draw() const override;

    const util::VertexStorage& storage() const { return _gpuStorage; }
    util::VertexStorage& storage() { return _gpuStorage; }
};

} // namespace mc

#endif /* triangle_soup_hpp */
