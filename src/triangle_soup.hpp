//
//  triangle_soup.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef triangle_soup_hpp
#define triangle_soup_hpp

#include "storage.hpp"

struct Triangle {
    Vertex a, b, c;

    Triangle(const Vertex& a, const Vertex& b, const Vertex& c)
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

    /**
     Signal that a batch of triangles will begin. Clears internal storage.
     */
    virtual void start() = 0;

    /**
     Add a triangle to storage
     */
    virtual void addTriangle(const Triangle& t) = 0;

    /**
     Signal that batch addition has completed; submits new triangles to GPU
     */
    virtual void finish() = 0;

    /**
     Draw the internal storage of triangles
     */
    virtual void draw() const = 0;
};

/**
 Consumes triangles and maintaining a non-indexed vertex storage
 */
class TriangleConsumer : public ITriangleConsumer {
private:
    std::vector<Vertex> _vertices;
    VertexStorage _gpuStorage { GL_TRIANGLES };

public:
    TriangleConsumer() = default;
    virtual ~TriangleConsumer() = default;

    void start() override;
    void addTriangle(const Triangle& t) override;
    void finish() override;
    void draw() const override;

    const VertexStorage& storage() const { return _gpuStorage; }
    VertexStorage& storage() { return _gpuStorage; }
};

class IndexedTriangleConsumer : public ITriangleConsumer {
public:
    enum class Strategy {
        // does nothing clever; only merges vertexes if identical
        // this means that adjacent triangles with differing normals
        // will not be merged.
        Basic,

        // attempts to merge shared triangle vertexes if their
        // normal dot products are within a threshold; the color
        // value of these vertexes will be averaged
        // TODO(shamyl@gmail.com): Unimplemented!
        NormalSmoothing
    };

private:
    std::vector<Vertex> _vertices;
    IndexedVertexStorage _gpuStorage { GL_TRIANGLES };
    Strategy _strategy;

public:
    IndexedTriangleConsumer(Strategy strategy)
        : _strategy(strategy)
    {
    }
    virtual ~IndexedTriangleConsumer() = default;

    void start() override;
    void addTriangle(const Triangle& t) override;
    void finish() override;
    void draw() const override;

    const IndexedVertexStorage& storage() const { return _gpuStorage; }
    IndexedVertexStorage& storage() { return _gpuStorage; }

private:
    void _stitch_Basic();
    void _stitch_NormalSmoothing();
};

#endif /* triangle_soup_hpp */
