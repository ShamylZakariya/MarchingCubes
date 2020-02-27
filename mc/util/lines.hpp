#ifndef mc_lines_h
#define mc_lines_h

#include "aabb.hpp"
#include "storage.hpp"

namespace mc {
namespace util {

    class LineSegmentBuffer {
    public:
        using vertex_type = VertexP3C4;

        LineSegmentBuffer() = default;
        LineSegmentBuffer(const LineSegmentBuffer&) = delete;
        LineSegmentBuffer(const LineSegmentBuffer&&) = delete;
        ~LineSegmentBuffer() = default;

        void clear()
        {
            _vertices.clear();
            _dirty = true;
        }

        void add(const vertex_type& a, const vertex_type& b)
        {
            _vertices.push_back(a);
            _vertices.push_back(b);
            _dirty = true;
        }

        template <typename T, glm::qualifier Q>
        inline void add(const AABB_<T, Q>& bounds, const glm::vec4& color)
        {
            auto corners = bounds.corners();

            // trace bottom
            add(vertex_type { corners[0], color }, vertex_type { corners[1], color });
            add(vertex_type { corners[1], color }, vertex_type { corners[2], color });
            add(vertex_type { corners[2], color }, vertex_type { corners[3], color });
            add(vertex_type { corners[3], color }, vertex_type { corners[0], color });

            // trace top
            add(vertex_type { corners[4], color }, vertex_type { corners[5], color });
            add(vertex_type { corners[5], color }, vertex_type { corners[6], color });
            add(vertex_type { corners[6], color }, vertex_type { corners[7], color });
            add(vertex_type { corners[7], color }, vertex_type { corners[4], color });

            // add bars connecting bottom to top
            add(vertex_type { corners[0], color }, vertex_type { corners[4], color });
            add(vertex_type { corners[1], color }, vertex_type { corners[5], color });
            add(vertex_type { corners[2], color }, vertex_type { corners[6], color });
            add(vertex_type { corners[3], color }, vertex_type { corners[7], color });
        }

        void addAxis(const glm::mat4& basis, float size)
        {
            const auto X = glm::vec3 { basis[0][0], basis[1][0], basis[2][0] };
            const auto Y = glm::vec3 { basis[0][1], basis[1][1], basis[2][1] };
            const auto Z = glm::vec3 { basis[0][2], basis[1][2], basis[2][2] };
            const auto pos = glm::vec3 { basis[3][0], basis[3][1], basis[3][2] };
            const auto red = glm::vec4 { 1, 0, 0, 1 };
            const auto green = glm::vec4 { 0, 1, 0, 1 };
            const auto blue = glm::vec4 { 0, 0, 1, 1 };
            add(vertex_type { pos, red }, vertex_type { pos + X * size, red });
            add(vertex_type { pos, green }, vertex_type { pos + Y * size, green });
            add(vertex_type { pos, blue }, vertex_type { pos + Z * size, blue });
        }

        void addMarker(const glm::vec3& pos, float size, const glm::vec4& color)
        {
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(-size, 0, 0), color });
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(+size, 0, 0), color });
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(0, -size, 0), color });
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(0, +size, 0), color });
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(0, 0, -size), color });
            add(vertex_type { pos, color }, vertex_type { pos + glm::vec3(0, 0, +size), color });
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
        std::vector<vertex_type> _vertices;
        VertexStorage<vertex_type> _gpuStorage { GL_LINES };
    };

    class LineStripBuffer {
    public:
        using vertex_type = VertexP3C4;

        LineStripBuffer() = default;
        LineStripBuffer(const LineStripBuffer&) = delete;
        LineStripBuffer(const LineStripBuffer&&) = delete;
        ~LineStripBuffer() = default;

        void clear()
        {
            _vertices.clear();
            _dirty = true;
        }

        void add(const vertex_type& a)
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
        std::vector<vertex_type> _vertices;
        VertexStorage<vertex_type> _gpuStorage { GL_LINE_STRIP };
    };

}
} // namespace mc::util

#endif /* mc_lines_h */