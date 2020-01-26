//
//  aabb.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_aabb_h
#define mc_aabb_h

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <ostream>

namespace mc {
namespace util {

    template <typename T, glm::qualifier Q = glm::defaultp>
    class AABB_ {
    public:
        enum class Intersection {
            Inside, // The object being tested is entirely inside the testing object
            Intersects, // The object being tested intersects the bounds of the testing object
            Outside // The object being tested is entirely outside the testing object
        };

        typedef T value_type;

        /*
        Create a default AABB_. Will return true for invalid() because
        it hasn't been assigned any value yet.
    */
        AABB_()
            : min(std::numeric_limits<T>::max())
            , max(-std::numeric_limits<T>::max())
        {
        }

        template <typename V>
        AABB_(const AABB_<V, Q>& other)
            : min(other.min)
            , max(other.max)
        {
        }

        AABB_(const glm::vec<3, T, Q>& min, const glm::vec<3, T, Q>& max)
            : min(min)
            , max(max)
        {
        }

        AABB_(const glm::vec<3, T, Q>& c, const T radius)
            : min(c.x - radius, c.y - radius, c.z - radius)
            , max(c.x + radius, c.y + radius, c.z + radius)
        {
        }

        bool operator==(const AABB_<T, Q>& other) const
        {
            return min == other.min && max == other.max;
        }

        bool operator!=(const AABB_<T, Q>& other) const
        {
            return min != other.min || max != other.max;
        }

        /*
        Two circumstances for "invalid" AABB_. It could be collapsed, where width,
        breadth or height are less than or equal to zero, or it is default constructed
        and not yet assigned values.
    */

        bool valid() const
        {
            return (min.x < max.x) && (min.y < max.y) && (min.z < max.z);
        }

        /*
        return true if this AABB is valid, e.g, not collapsed and has been expanded to fit something.
    */
        operator bool() const
        {
            return valid();
        }

        /*
        Make this AABB_ invalid e.g., just like a default-constructed AABB_.
        Will fail the valid test.
    */
        void invalidate()
        {
            min.x = std::numeric_limits<T>::max();
            max.x = -std::numeric_limits<T>::max();
            min.y = std::numeric_limits<T>::max();
            max.y = -std::numeric_limits<T>::max();
            min.z = std::numeric_limits<T>::max();
            max.z = -std::numeric_limits<T>::max();
        }

        /*
        return geometric center of AABB_
    */
        glm::vec<3, T, Q> center() const
        {
            return glm::vec<3, T, Q>(
                (min.x + max.x) / 2,
                (min.y + max.y) / 2,
                (min.z + max.z) / 2);
        }

        /*
        return the radius of the sphere that would exactly enclose this AABB_
    */
        T radius() const
        {
            return glm::length((max - min) * 0.5F);
        }

        /*
        return the squared radius of the sphere that would exactly enclose this AABB_
    */
        T radius2() const
        {
            return glm::length2((max - min) * 0.5F);
        }

        /*
        return the magnitude along the 3 axes.
    */
        glm::vec<3, T, Q> size() const { return glm::vec<3, T, Q>(max.x - min.x, max.y - min.y, max.z - min.z); }

        /*
        return the volume displaced by this aabb
    */
        T volume() const
        {
            auto sz = size();
            return sz.x * sz.y * sz.z;
        }

        /*
        return AABB_ containing both
    */
        const AABB_<T, Q> operator+(const AABB_<T, Q>& a) const
        {
            return AABB_<T, Q>(
                std::min<T>(min.x, a.min.x),
                std::max<T>(max.x, a.max.x),
                std::min<T>(min.y, a.min.y),
                std::max<T>(max.y, a.max.y),
                std::min<T>(min.z, a.min.z),
                std::max<T>(max.z, a.max.z));
        }

        /*
        return AABB_ containing both this AABB_ and the point
    */
        const AABB_<T, Q> operator+(const glm::vec<3, T, Q>& p) const
        {
            return AABB_<T, Q>(
                std::min<T>(min.x, p.x),
                std::max<T>(max.x, p.x),
                std::min<T>(min.y, p.y),
                std::max<T>(max.y, p.y),
                std::min<T>(min.z, p.z),
                std::max<T>(max.z, p.z));
        }

        /*
        Expand AABB_ by scalar value
    */
        const AABB_<T, Q> operator+(const T d) const
        {
            return AABB_<T, Q>(
                min.x - d, max.x + d,
                min.y - d, max.y + d,
                min.z - d, max.z + d);
        }

        /*
        Inset AABB_ by scalar value
    */
        const AABB_<T, Q> operator-(const T d) const
        {
            return AABB_<T, Q>(
                min.x + d, max.x - d,
                min.y + d, max.y - d,
                min.z + d, max.z - d);
        }

        /*
        Make this AABB_ become the add of it and the other AABB_
    */
        AABB_<T, Q>& operator+=(const AABB_<T, Q>& a)
        {
            min.x = std::min(min.x, a.min.x),
            max.x = std::max(max.x, a.max.x),
            min.y = std::min(min.y, a.min.y),
            max.y = std::max(max.y, a.max.y),
            min.z = std::min(min.z, a.min.z),
            max.z = std::max(max.z, a.max.z);

            return *this;
        }

        /*
        Make this AABB_ become the union of it and the other AABB_
    */
        void add(const AABB_<T, Q>& a)
        {
            min.x = std::min(min.x, a.min.x),
            max.x = std::max(max.x, a.max.x),
            min.y = std::min(min.y, a.min.y),
            max.y = std::max(max.y, a.max.y),
            min.z = std::min(min.z, a.min.z),
            max.z = std::max(max.z, a.max.z);
        }

        /*
        Expand this AABB_ ( if necessary ) to contain the given point
    */
        AABB_<T, Q>& operator+=(const glm::vec<3, T, Q>& p)
        {
            min.x = std::min(min.x, p.x);
            max.x = std::max(max.x, p.x);
            min.y = std::min(min.y, p.y);
            max.y = std::max(max.y, p.y);
            min.z = std::min(min.z, p.z);
            max.z = std::max(max.z, p.z);

            return *this;
        }

        /*
        Expand this AABB_ ( if necessary ) to contain the given point
    */
        void add(const glm::vec<3, T, Q>& p)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        /*
        Expand this AABB_ ( if necessary ) ton contain the given sphere
    */
        void add(const glm::vec<3, T, Q>& p, T radius)
        {
            min.x = std::min(min.x, p.x - radius);
            max.x = std::max(max.x, p.x + radius);
            min.y = std::min(min.y, p.y - radius);
            max.y = std::max(max.y, p.y + radius);
            min.z = std::min(min.z, p.z - radius);
            max.z = std::max(max.z, p.z + radius);
        }

        /*
        Outset this AABB_ by scalar factor
    */
        AABB_<T, Q>& operator+=(const T d)
        {
            min.x -= d;
            max.x += d;
            min.y -= d;
            max.y += d;
            min.z -= d;
            max.z += d;

            return *this;
        }

        /*
        Outset this AABB_ by scalar factor
    */
        void outset(T d)
        {
            min.x -= d;
            min.y -= d;
            min.z -= d;
            max.x += d;
            max.y += d;
            max.z += d;
        }

        /*
        Inset this AABB_ by scalar factor
    */
        AABB_<T, Q>& operator-=(const T d)
        {
            min.x += d;
            max.x -= d;
            min.y += d;
            max.y -= d;
            min.z += d;
            max.z -= d;

            return *this;
        }

        /*
        Inset this AABB_ by scalar factor
    */
        void inset(T d)
        {
            min.x += d;
            min.y += d;
            min.z += d;
            max.x -= d;
            max.y -= d;
            max.z -= d;
        }

        /*
        Transform this AABB_ by p
    */
        void translate(const glm::vec<3, T, Q>& p)
        {
            min += p;
            max += p;
        }

        /*
        return true if point is in this AABB_
    */
        bool contains(const glm::vec<3, T, Q>& point) const
        {
            return (point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z && point.z <= max.z);
        }

        std::array<glm::vec<3, T, Q>, 8> corners() const
        {
            return std::array<glm::vec<3, T, Q>, 8> {
                glm::vec<3, T, Q>(min.x, min.y, min.z),
                glm::vec<3, T, Q>(min.x, min.y, max.z),
                glm::vec<3, T, Q>(max.x, min.y, max.z),
                glm::vec<3, T, Q>(max.x, min.y, min.z),
                glm::vec<3, T, Q>(min.x, max.y, min.z),
                glm::vec<3, T, Q>(min.x, max.y, max.z),
                glm::vec<3, T, Q>(max.x, max.y, max.z),
                glm::vec<3, T, Q>(max.x, max.y, min.z)
            };
        }

        void corners(std::array<glm::vec<3, T, Q>, 8>& c) const
        {
            c[0] = glm::vec<3, T, Q>(min.x, min.y, min.z);
            c[1] = glm::vec<3, T, Q>(min.x, min.y, max.z);
            c[2] = glm::vec<3, T, Q>(max.x, min.y, max.z);
            c[3] = glm::vec<3, T, Q>(max.x, min.y, min.z);
            c[4] = glm::vec<3, T, Q>(min.x, max.y, min.z);
            c[5] = glm::vec<3, T, Q>(min.x, max.y, max.z);
            c[6] = glm::vec<3, T, Q>(max.x, max.y, max.z);
            c[7] = glm::vec<3, T, Q>(max.x, max.y, min.z);
        }

        /*
        return the intersection type of this AABB_ with other
    */
        Intersection intersect(const AABB_<T, Q>& other) const
        {
            // Check first to see if 'other' is completely inside this AABB_.
            // If it is, return Inside.

            if (other.min.x >= min.x && other.max.x <= max.x && other.min.y >= min.y && other.max.y <= max.y && other.min.z >= min.z && other.max.z <= max.z) {
                return Intersection::Inside;
            }

            // If we're here, determine if there's any overlap

            bool xOverlap = (min.x < other.max.x && max.x > other.min.x);
            bool yOverlap = (min.y < other.max.y && max.y > other.min.y);
            bool zOverlap = (min.z < other.max.z && max.z > other.min.z);

            return (xOverlap && yOverlap && zOverlap) ? Intersection::Intersects : Intersection::Outside;
        }

        /*
        return the intersection type of this AABB_ with the sphere at center with given radius
    */
        Intersection intersect(const glm::vec<3, T, Q>& center, T radius) const
        {
            return intersects(AABB_<T, Q>(center, radius));
        }

        /**
     * Subdivides this AABB into 8 child AABBs
     */
        void octreeSubdivide(std::array<AABB_<T, Q>, 8>& into) const
        {
            auto min = this->min;
            auto size = this->size();

            // no divide exists for glm::ivec3, so we do it explicitly
            auto hx = size.x / 2;
            auto hy = size.y / 2;
            auto hz = size.z / 2;
            auto hs = glm::vec<3, T, Q>(hx, hy, hz);

            auto a = min + glm::vec<3, T, Q>(+0, 0, +0);
            auto b = min + glm::vec<3, T, Q>(+hx, 0, +0);
            auto c = min + glm::vec<3, T, Q>(+hx, 0, +hz);
            auto d = min + glm::vec<3, T, Q>(+0, 0, +hz);

            auto e = min + glm::vec<3, T, Q>(+0, +hy, +0);
            auto f = min + glm::vec<3, T, Q>(+hx, +hy, +0);
            auto g = min + glm::vec<3, T, Q>(+hx, +hy, +hz);
            auto h = min + glm::vec<3, T, Q>(+0, +hy, +hz);

            into[0] = AABB_<T, Q>(a, a + hs);
            into[1] = AABB_<T, Q>(b, b + hs);
            into[2] = AABB_<T, Q>(c, c + hs);
            into[3] = AABB_<T, Q>(d, d + hs);

            into[4] = AABB_<T, Q>(e, e + hs);
            into[5] = AABB_<T, Q>(f, f + hs);
            into[6] = AABB_<T, Q>(g, g + hs);
            into[7] = AABB_<T, Q>(h, h + hs);
        }

        std::array<AABB_<T, Q>, 8> octreeSubdivide() const
        {
            auto store = std::array<AABB_<T, Q>, 8> {};
            octreeSubdivide(store);
            return store;
        }

    public:
        glm::vec<3, T, Q> min, max;
    };

    typedef AABB_<float, glm::defaultp> AABB;
    typedef AABB_<glm::int32, glm::defaultp> iAABB;

}
} // namespace mc::util

namespace std {
template <typename T, glm::qualifier Q>
struct hash<mc::util::AABB_<T, Q>> {
    size_t operator()(mc::util::AABB_<T, Q> const& aabb) const
    {
        // https://en.cppreference.com/w/cpp/utility/hash
        size_t h0 = hash<glm::vec<3, T, Q>>()(aabb.min);
        size_t h1 = hash<glm::vec<3, T, Q>>()(aabb.max);
        return (h0 ^ (h1 << 1));
    }
};
}

template <typename T, glm::qualifier Q>
std::ostream& operator<<(std::ostream& os, const mc::util::AABB_<T, Q>& bounds)
{
    os << "[AABB min(" << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z
       << ") max(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z
       << ") size(" << bounds.size().x << ", " << bounds.size().y << ", " << bounds.size().z
       << ")]";

    return os;
}

#endif /* mc_aabb_h */
