//
//  util.h
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef util_h
#define util_h

#include <limits>

#include <epoxy/gl.h>
// #include <epoxy/glx.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>

#include "thread_pool.hpp"
#include "unowned_ptr.hpp"

using namespace glm;

namespace util {

/**
 Read a file into a string
 */
std::string ReadFile(const std::string& filename);

/**
 Loads image into a texture 2d
 */
GLuint LoadTexture(const std::string& filename);

/**
 Checks for GL error, throwing if there is one
 */
void CheckGlError(const char* ctx);

/**
 Creates a shader of specified type from provided source
 */
GLuint CreateShader(GLenum shader_type, const char* src);

/**
 Creates full shader program from vertex and fragment sources
 */
GLuint CreateProgram(const char* vtxSrc, const char* fragSrc);

namespace color {

    // cribbed from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both

    struct rgb {
        float r; // a fraction between 0 and 1
        float g; // a fraction between 0 and 1
        float b; // a fraction between 0 and 1

        rgb()
            : r(0)
            , g(0)
            , b(0)
        {
        }

        rgb(float R, float G, float B)
            : r(R)
            , g(G)
            , b(B)
        {
        }

        explicit operator vec3() const
        {
            return vec3(r, g, b);
        }
    };

    struct hsv {
        float h; // angle in degrees (0,360)
        float s; // a fraction between 0 and 1
        float v; // a fraction between 0 and 1

        hsv()
            : h(0)
            , s(0)
            , v(0)
        {
        }

        hsv(float H, float S, float V)
            : h(H)
            , s(S)
            , v(V)
        {
        }
    };

    hsv Rgb2Hsv(rgb in);
    rgb Hsv2Rgb(hsv in);
}

class FpsCalculator {
public:
    FpsCalculator() { reset(); }
    FpsCalculator(const FpsCalculator&) = delete;
    FpsCalculator(const FpsCalculator&&) = delete;
    ~FpsCalculator() = default;

    void reset()
    {
        _warmUp = true;
        _lastTimestamp = glfwGetTime();
        _fpsSum = 0;
        _fpsCount = 0;
        _averageFps = 0;
        _minFrameTime = std::numeric_limits<double>::max();
        _maxFrameTime = 0;
    }

    double update()
    {
        static constexpr int FPS_SAMPLES = 30;
        auto now = glfwGetTime();
        auto delta = now - _lastTimestamp;

        _fpsSum += 1.0F / delta;
        ++_fpsCount;

        if (_fpsCount == FPS_SAMPLES) {
            _averageFps = _fpsSum / _fpsCount;
            _fpsSum = 0;
            _fpsCount = 0;
            _minFrameTime = std::min(_minFrameTime, delta);
            _maxFrameTime = std::max(_minFrameTime, delta);
            _warmUp = false;
        } else if (_warmUp) {
            _averageFps = _fpsSum / _fpsCount;
        }

        _lastTimestamp = now;
        return delta;
    }

    double getFps() const { return _averageFps; }
    double getMinFrameTime() const { return _minFrameTime; }
    double getMaxFrameTime() const { return _maxFrameTime; }

private:
    bool _warmUp = true;
    double _lastTimestamp = 0;
    double _fpsSum = 0;
    int _fpsCount = 0;
    double _averageFps = 0;
    double _minFrameTime = 0;
    double _maxFrameTime = 0;
};

}

#endif /* util_h */
