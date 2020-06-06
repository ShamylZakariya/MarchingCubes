#ifndef camera_hpp
#define camera_hpp

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mc/util/aabb.hpp"

struct Frustum {
public:
    enum class Intersection {
        Outside,
        Inside,
        Intersects
    };

    void set(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& org)
    {
        // https://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
        glm::mat4 clip(projection * view);
        const float* mat = glm::value_ptr(clip);

        _left.set(mat[3] + mat[0],
            mat[7] + mat[4],
            mat[11] + mat[8],
            mat[15] + mat[12]);

        _right.set(mat[3] - mat[0],
            mat[7] - mat[4],
            mat[11] - mat[8],
            mat[15] - mat[12]);

        _bottom.set(mat[3] + mat[1],
            mat[7] + mat[5],
            mat[11] + mat[9],
            mat[15] + mat[13]);

        _top.set(mat[3] - mat[1],
            mat[7] - mat[5],
            mat[11] - mat[9],
            mat[15] - mat[13]);

        _near.set(mat[3] + mat[2],
            mat[7] + mat[6],
            mat[11] + mat[10],
            mat[15] + mat[14]);

        _far.set(mat[3] - mat[2],
            mat[7] - mat[6],
            mat[11] - mat[10],
            mat[15] - mat[14]);

        _origin = org;
    }

    Intersection intersect(const mc::util::AABB& bounds) const
    {
        using glm::vec3;

        // early exit
        if (bounds.contains(_origin)) {
            return Intersection::Intersects;
        }

        const vec3 points[8] = {
            vec3(bounds.min.x, bounds.min.y, bounds.min.z),
            vec3(bounds.min.x, bounds.max.y, bounds.min.z),
            vec3(bounds.max.x, bounds.max.y, bounds.min.z),
            vec3(bounds.max.x, bounds.min.y, bounds.min.z),

            vec3(bounds.min.x, bounds.min.y, bounds.max.z),
            vec3(bounds.min.x, bounds.max.y, bounds.max.z),
            vec3(bounds.max.x, bounds.max.y, bounds.max.z),
            glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z)
        };

        int rightStatus = 0,
            leftStatus = 0,
            bottomStatus = 0,
            topStatus = 0,
            farStatus = 0,
            nearStatus = 0;

        for (int i = 0; i < 8; i++) {
            nearStatus += _near.inFront(points[i]) ? 1 : 0;
        }
        if (nearStatus == 0) {
            return Intersection::Outside;
        }

        for (int i = 0; i < 8; i++) {
            rightStatus += _right.inFront(points[i]) ? 1 : 0;
        }
        if (rightStatus == 0) {
            return Intersection::Outside;
        }

        for (int i = 0; i < 8; i++) {
            leftStatus += _left.inFront(points[i]) ? 1 : 0;
        }
        if (leftStatus == 0) {
            return Intersection::Outside;
        }

        for (int i = 0; i < 8; i++) {
            bottomStatus += _bottom.inFront(points[i]) ? 1 : 0;
        }
        if (bottomStatus == 0) {
            return Intersection::Outside;
        }

        for (int i = 0; i < 8; i++) {
            topStatus += _top.inFront(points[i]) ? 1 : 0;
        }
        if (topStatus == 0) {
            return Intersection::Outside;
        }

        for (int i = 0; i < 8; i++) {
            farStatus += _far.inFront(points[i]) ? 1 : 0;
        }
        if (farStatus == 0) {
            return Intersection::Outside;
        }

        if (rightStatus == 8 && leftStatus == 8 && bottomStatus == 8 && topStatus == 8 && farStatus == 8 && nearStatus == 8)
            return Intersection::Inside;

        return Intersection::Intersects;
    }

private:
    // minimalist plane definition to represent frustum side
    struct plane {

        void set(float A, float B, float C, float D)
        {
            float rmag = 1.0f / std::sqrt(A * A + B * B + C * C);
            a = A * rmag;
            b = B * rmag;
            c = C * rmag;
            d = D * rmag;
        }

        float distance(const glm::vec3& p) const
        {
            return a * p.x + b * p.y + c * p.z + d;
        }

        bool inFront(const glm::vec3& p) const { return distance(p) > 0; }

        float a = 0;
        float b = 0;
        float c = 0;
        float d = 0;
    };

    plane _right, _left, _bottom, _top, _near, _far;
    glm::vec3 _origin;
};

struct Camera {
public:
    const glm::mat4& getProjection() const { return _projection; }

    glm::mat4 getView() const
    {
        using namespace glm;
        auto up = vec3 { _look[0][1], _look[1][1], _look[2][1] };
        auto forward = vec3 { _look[0][2], _look[1][2], _look[2][2] };
        return glm::lookAt(_position, _position + forward, up);
    }

    glm::vec3 getPosition() const { return _position; }

    glm::vec3 getForward() const
    {
        return glm::vec3(
            _look[0][2],
            _look[1][2],
            _look[2][2]);
    }

    void moveBy(glm::vec3 deltaLocal)
    {
        using namespace glm;
        vec3 deltaWorld = inverse(_look) * deltaLocal;
        _position += deltaWorld;
    }

    void rotateBy(float yaw, float pitch)
    {
        using namespace glm;
        const auto right = vec3 { _look[0][0], _look[1][0], _look[2][0] };
        _look = rotate(mat4{_look}, pitch, right);
        _look = rotate(mat4{_look}, yaw, vec3{0,1,0});
    }

    void lookAt(glm::vec3 position, glm::vec3 at, glm::vec3 up = glm::vec3(0, 1, 0))
    {
        using namespace glm;
        _position = position;

        vec3 forward = normalize(at - position);
        vec3 right = cross(up, forward);

        _look[0][0] = right.x;
        _look[1][0] = right.y;
        _look[2][0] = right.z;
        _look[0][1] = up.x;
        _look[1][1] = up.y;
        _look[2][1] = up.z;
        _look[0][2] = forward.x;
        _look[1][2] = forward.y;
        _look[2][2] = forward.z;
    }

    void updateProjection(int viewWidth, int viewHeight, float fovDegrees, float nearPlane, float farPlane)
    {
        auto aspect = static_cast<float>(viewWidth) / static_cast<float>(viewHeight);
        _projection = glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
    }

    void updateFrustum()
    {
        _frustum.set(_projection, getView(), _position);
    }

    const Frustum& getFrustum() const
    {
        return _frustum;
    }

private:
    glm::mat3 _look = glm::mat3 { 1 };
    glm::vec3 _position = glm::vec3 { 0, 0, -100 };
    glm::mat4 _projection;
    Frustum _frustum;
};

#endif