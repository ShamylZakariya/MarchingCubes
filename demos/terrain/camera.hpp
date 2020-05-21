#ifndef camera_hpp
#define camera_hpp

#include <glm/glm.hpp>

struct Camera {
private:
    glm::mat3 _look = glm::mat3 { 1 };
    glm::vec3 _position = glm::vec3 { 0, 0, -100 };
public:
    glm::mat4 view() const
    {
        using namespace glm;
        auto up = vec3 { _look[0][1], _look[1][1], _look[2][1] };
        auto forward = vec3 { _look[0][2], _look[1][2], _look[2][2] };
        return glm::lookAt(_position, _position + forward, up);
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
        auto right = vec3 { _look[0][0], _look[1][0], _look[2][0] };
        _look = mat4 { _look } * rotate(rotate(mat4 { 1 }, yaw, vec3 { 0, 1, 0 }), pitch, right);
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

    glm::vec3 getPosition() const { return _position; }

    glm::vec3 getForward() const {
        return glm::vec3(
            _look[0][2],
            _look[1][2],
            _look[2][2]
        );
    }
};

#endif