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

    template <typename T, qualifier Q>
    inline void add(const AABB_<T, Q>& bounds, const vec4& color)
    {
        auto corners = bounds.corners();

        // trace bottom
        add(Vertex { corners[0], color }, Vertex { corners[1], color });
        add(Vertex { corners[1], color }, Vertex { corners[2], color });
        add(Vertex { corners[2], color }, Vertex { corners[3], color });
        add(Vertex { corners[3], color }, Vertex { corners[0], color });

        // trace top
        add(Vertex { corners[4], color }, Vertex { corners[5], color });
        add(Vertex { corners[5], color }, Vertex { corners[6], color });
        add(Vertex { corners[6], color }, Vertex { corners[7], color });
        add(Vertex { corners[7], color }, Vertex { corners[4], color });

        // add bars connecting bottom to top
        add(Vertex { corners[0], color }, Vertex { corners[4], color });
        add(Vertex { corners[1], color }, Vertex { corners[5], color });
        add(Vertex { corners[2], color }, Vertex { corners[6], color });
        add(Vertex { corners[3], color }, Vertex { corners[7], color });
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

#endif /* lines_hpp */