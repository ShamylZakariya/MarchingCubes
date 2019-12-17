//
//  aabb.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/25/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef AABB_h
#define AABB_h

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <algorithm>
#include <limits>

using namespace glm;

template <typename T, qualifier Q = defaultp>
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
    AABB_(void)
        : min(std::numeric_limits<T>::max())
        , max(-std::numeric_limits<T>::max())
    {
    }

    AABB_(const AABB_<T, Q>& other)
        : min(other.min)
        , max(other.max)
    {
    }

    AABB_(const vec<3, T, Q>& min, const vec<3, T, Q>& max)
        : min(min)
        , max(max)
    {
    }

    AABB_(const vec<3, T, Q>& c, const T radius)
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

    bool valid(void) const
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
    void invalidate(void)
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
    vec<3, T, Q> center(void) const
    {
        return vec<3, T, Q>(
            (min.x + max.x) / 2,
            (min.y + max.y) / 2,
            (min.z + max.z) / 2);
    }

    /*
        return the radius of the sphere that would exactly enclose this AABB_
    */
    T radius(void) const
    {
        T xSize(max.x - min.x), ySize(max.y - min.y), zSize(max.z - min.z);

        xSize /= 2;
        ySize /= 2;
        zSize /= 2;

        return sqrt(xSize * xSize + ySize * ySize + zSize * zSize);
    }

    /*
        return the magnitude along the 3 axes.
    */
    vec<3, T, Q> size(void) const { return vec<3, T, Q>(max.x - min.x, max.y - min.y, max.z - min.z); }

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
    const AABB_<T, Q> operator+(const vec<3, T, Q>& p) const
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
    AABB_<T, Q>& operator+=(const vec<3, T, Q>& p)
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
    void add(const vec<3, T, Q>& p)
    {
        min.x = std::min(min.x, p.x);
        max.x = std::max(max.x, p.x);
        min.y = std::min(min.y, p.y);
        max.y = std::max(max.y, p.y);
        min.z = std::min(min.z, p.z);
        max.z = std::max(max.z, p.z);
    }

    /*
        Expand this AABB_ ( if necessary ) ton contain the given sphere
    */
    void add(const vec<3, T, Q>& p, T radius)
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
        max.x += d;
        min.y -= d;
        max.y += d;
        min.z -= d;
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
        min += vec3(d);
        max -= vec3(d);
    }

    /*
        Transform this AABB_ by p
    */
    void translate(const vec<3, T, Q>& p)
    {
        min += p;
        max += p;
    }

    /*
        return true if point is in this AABB_
    */
    bool contains(const vec<3, T, Q>& point) const
    {
        return (point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z && point.z <= max.z);
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
    Intersection intersect(const vec<3, T, Q>& center, T radius) const
    {
        return intersects(AABB_<T, Q>(center, radius));
    }

public:
    vec<3, T, Q> min, max;
};

typedef AABB_<float, defaultp> AABB;
typedef AABB_<int32, defaultp> AABBi;

namespace std {
template <typename T, qualifier Q>
struct hash<AABB_<T, Q>> {
    size_t operator()(AABB_<T, Q> const& aabb) const
    {
        // https://en.cppreference.com/w/cpp/utility/hash
        size_t h0 = hash<vec<3, T, Q>>()(aabb.min);
        size_t h1 = hash<vec<3, T, Q>>()(aabb.max);
        return (h0 ^ (h1 << 1));
    }
};
}

#endif /* AABB_h */
