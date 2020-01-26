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

class IndexedTriangleConsumer : public ITriangleConsumer {
public:
    enum class Strategy {
        // does nothing clever; only merges vertexes if identical
        // this means that adjacent triangles with differing normals
        // or colors will not be merged.
        Basic,

        // attempts to merge shared triangle vertexes if the
        // colors are same but normals are within a threshold.
        // if normals are within a threshold, the generated
        // shared vertex will have the average of the contributing
        // normals
        NormalSmoothing
    };

private:
    std::vector<util::Vertex> _vertices;
    util::IndexedVertexStorage _gpuStorage { GL_TRIANGLES };
    Strategy _strategy;
    size_t _numTriangles;
    float _normalSmoothingDotThreshold = 0.96F; // ~15°

public:
    IndexedTriangleConsumer(Strategy strategy)
        : _strategy(strategy)
        , _numTriangles(0)
    {
    }
    virtual ~IndexedTriangleConsumer() = default;

    void start() override;
    void addTriangle(const Triangle& t) override;
    size_t numTriangles() const override { return _numTriangles; }
    void finish() override;
    void draw() const override;

    const util::IndexedVertexStorage& storage() const { return _gpuStorage; }
    util::IndexedVertexStorage& storage() { return _gpuStorage; }

    /*
     If triangles share a point, the point will be shared iff the triangle's normals for that point
     are within this angular difference. In short, this allows automatic smoothing of normals when
     triangles touch if they have similar normals; but if not, unique vertices will be used.
     Only applies to Strategy::NormalSmoothing
     */
    void setNormalSmoothingCreaseThresholdRadians(float normalSmoothingCreaseThresholdRadians);
    float normalSmoothingThresholdRadians() const;

private:
    void _stitch_Basic();
    void _stitch_NormalSmoothing();
};

} // namespace mc

#endif /* triangle_soup_hpp */
