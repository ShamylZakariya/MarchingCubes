#ifndef camera_hpp
#define camera_hpp

#include <glm/glm.hpp>

struct Camera {
    glm::mat3 look = glm::mat3 { 1 };
    glm::vec3 position = glm::vec3 { 0, 0, -100 };

    glm::mat4 view() const
    {
        using namespace glm;
        auto up = vec3 { look[0][1], look[1][1], look[2][1] };
        auto forward = vec3 { look[0][2], look[1][2], look[2][2] };
        return glm::lookAt(position, position + forward, up);
    }

    void moveBy(glm::vec3 deltaLocal)
    {
        using namespace glm;
        vec3 deltaWorld = inverse(look) * deltaLocal;
        position += deltaWorld;
    }

    void rotateBy(float yaw, float pitch)
    {
        using namespace glm;
        auto right = vec3 { look[0][0], look[1][0], look[2][0] };
        look = mat4 { look } * rotate(rotate(mat4 { 1 }, yaw, vec3 { 0, 1, 0 }), pitch, right);
    }

    void lookAt(glm::vec3 position, glm::vec3 at, glm::vec3 up = glm::vec3(0, 1, 0))
    {
        using namespace glm;
        this->position = position;

        vec3 forward = normalize(at - position);
        vec3 right = cross(up, forward);

        look[0][0] = right.x;
        look[1][0] = right.y;
        look[2][0] = right.z;
        look[0][1] = up.x;
        look[1][1] = up.y;
        look[2][1] = up.z;
        look[0][2] = forward.x;
        look[1][2] = forward.y;
        look[2][2] = forward.z;
    }
};

#endif