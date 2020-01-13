#ifndef lines_hpp
#define lines_hpp

#include "aabb.hpp"
#include "storage.hpp"

class LineSegmentBuffer {
public:
    LineSegmentBuffer() = default;
    LineSegmentBuffer(const LineSegmentBuffer&) = delete;
    LineSegmentBuffer(const LineSegmentBuffer&&) = delete;
    ~LineSegmentBuffer() = default;

    void clear()
    {
        _vertices.clear();
        _dirty = true;
    }

    void add(const Vertex& a, const Vertex& b)
    {
        _vertices.push_back(a);
        _vertices.push_back(b);
        _dirty = true;
    }

    void draw()
    {
        if (_dirty) {
            _gpuStorage.update(_vertices);
            _dirty = false;
        }
        _gpuStorage.draw();
    }

private:
    bool _dirty = false;
    std::vector<Vertex> _vertices;
    VertexStorage _gpuStorage { GL_LINES };
};

class LineStripBuffer {
public:
    LineStripBuffer() = default;
    LineStripBuffer(const LineStripBuffer&) = delete;
    LineStripBuffer(const LineStripBuffer&&) = delete;
    ~LineStripBuffer() = default;

    void clear()
    {
        _vertices.clear();
        _dirty = true;
    }

    void add(const Vertex& a)
    {
        _vertices.push_back(a);
        _dirty = true;
    }

    void close()
    {
        _vertices.push_back(_vertices.front());
        _dirty = true;
    }

    void draw()
    {
        if (_dirty) {
            _gpuStorage.update(_vertices);
            _dirty = false;
        }
        _gpuStorage.draw();
    }

private:
    bool _dirty = false;
    std::vector<Vertex> _vertices;
    VertexStorage _gpuStorage { GL_LINE_STRIP };
};

template <typename T, qualifier Q>
inline void AppendAABB(const AABB_<T, Q>& bounds, LineSegmentBuffer& lineSegs, const vec4& color)
{
    auto vertices = bounds.vertices();

    // trace bottom
    lineSegs.add(Vertex { vertices[0], color }, Vertex { vertices[1], color });
    lineSegs.add(Vertex { vertices[1], color }, Vertex { vertices[2], color });
    lineSegs.add(Vertex { vertices[2], color }, Vertex { vertices[3], color });
    lineSegs.add(Vertex { vertices[3], color }, Vertex { vertices[0], color });

    // trace top
    lineSegs.add(Vertex { vertices[4], color }, Vertex { vertices[5], color });
    lineSegs.add(Vertex { vertices[5], color }, Vertex { vertices[6], color });
    lineSegs.add(Vertex { vertices[6], color }, Vertex { vertices[7], color });
    lineSegs.add(Vertex { vertices[7], color }, Vertex { vertices[4], color });

    // add bars connecting bottom to top
    lineSegs.add(Vertex { vertices[0], color }, Vertex { vertices[4], color });
    lineSegs.add(Vertex { vertices[1], color }, Vertex { vertices[5], color });
    lineSegs.add(Vertex { vertices[2], color }, Vertex { vertices[6], color });
    lineSegs.add(Vertex { vertices[3], color }, Vertex { vertices[7], color });

}

#endif /* lines_hpp */