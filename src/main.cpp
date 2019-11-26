//
//  main.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "marching_cubes.hpp"
#include "storage.hpp"
#include "triangle_soup.hpp"
#include "util.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma mark - Constants

const int WIDTH = 800;
const int HEIGHT = 600;
const float NEAR_PLANE = 0.1f;
const float FAR_PLANE = 1000.0f;
const float FOV_DEGREES = 50.0F;

#pragma mark - Data

struct ProgramState {
    GLuint program = 0;
    GLint uniformLocMVP = -1;
    GLint uniformLocModel = -1;

    ~ProgramState()
    {
        if (program > 0) {
            glDeleteProgram(program);
        }
    }

    void build(const std::string& vertSrc, const std::string& fragSrc)
    {
        program = util::CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        uniformLocModel = glGetUniformLocation(program, "uModel");
    }
};

#pragma mark - Simple IsoSurface Sampler

class Volume : public mc::IIsoSurface {
public:
    Volume(vec3 sphereCenter, float sphereRadius, float threshold)
        : _center(sphereCenter)
        , _radius(sphereRadius)
        , _threshold(threshold)
    {
    }

    ivec3 size() const override
    {
        return _center + vec3(_radius);
    }

    float valueAt(const vec3& p) const override
    {
        float d2 = distance2(p, _center);
        float min2 = _radius * _radius;
        if (d2 < min2) {
            return 1;
        }

        float max2 = (_radius + _threshold) * (_radius + _threshold);
        if (d2 > max2) {
            return 0;
        }

        float d = sqrt(d2) - _radius;
        return 1 - (d / _threshold);
    }

private:
    vec3 _center;
    float _radius;
    float _threshold;
};

class DummyConsumer : public ITriangleConsumer {
public:
    DummyConsumer() = default;

    void start() override
    {
        _triangleCount = 0;
    }

    void addTriangle(const Triangle& t) override
    {
        _triangleCount++;
    }

    void finish() override
    {
        std::cout << "Consumed " << _triangleCount << " triangles" << std::endl;
    }

    void draw() const override {}

private:
    size_t _triangleCount = 0;
};

#pragma mark - App

class OpenGLCubeApplication {
public:
    OpenGLCubeApplication()
    {
        initWindow();
        initGl();
        buildMesh();
    }

    ~OpenGLCubeApplication() = default;

    void run()
    {
        while (!glfwWindowShouldClose(_window)) {
            glfwPollEvents();
            drawFrame();
            glfwSwapBuffers(_window);
        }
    }

private:
    GLFWwindow* _window;

    ProgramState _program;
    IndexedTriangleConsumer _mcTriangleConsumer { IndexedTriangleConsumer::Strategy::Basic };

    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };

    mat4 _proj { 1 };
    mat4 _model { 1 };
    float _cameraZPosition = -4;

private:
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        _window = glfwCreateWindow(WIDTH, HEIGHT, "Marching Cubes", nullptr, nullptr);
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onResize(width, height);
        });

        glfwSetKeyCallback(_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            switch (action) {
            case GLFW_PRESS:
                app->onKeyPress(key, scancode, mods);
                break;
            case GLFW_RELEASE:
                app->onKeyRelease(key, scancode, mods);
                break;
            }
        });

        glfwSetMouseButtonCallback(_window, [](GLFWwindow* window, int button, int action, int mods) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            switch (action) {
            case GLFW_PRESS:
                app->onMouseDown(button, mods);
                break;
            case GLFW_RELEASE:
                app->onMouseUp(button, mods);
                break;
            }
        });

        glfwSetScrollCallback(_window, [](GLFWwindow* window, double xOffset, double yOffset) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onMouseWheel(vec2(xOffset, yOffset));
        });

        glfwSetCursorPosCallback(_window, [](GLFWwindow* window, double xPos, double yPos) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            vec2 pos { xPos, yPos };
            if (app->_lastMousePosition != vec2 { -1 }) {
                vec2 delta = pos - app->_lastMousePosition;
                app->onMouseMove(pos, delta);
            } else {
                app->onMouseMove(pos, vec2 { 0 });
            }
            app->_lastMousePosition = pos;
        });

        glfwMakeContextCurrent(_window);
    }

    void initGl()
    {

        //
        // Kick off glew
        //

        glewExperimental = true;
        if (glewInit() != GLEW_OK) {
            throw std::runtime_error("Failed to initialize GLEW\n");
        }

        std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL version supported: " << glGetString(GL_VERSION) << std::endl;

        //
        // load program
        //

        auto vertFile = "shaders/gl/vert.glsl";
        auto fragFile = "shaders/gl/frag.glsl";
        auto vertSrc = util::ReadFile(vertFile);
        auto fragSrc = util::ReadFile(fragFile);
        _program.build(vertSrc, fragSrc);

        //
        // some constant GL state
        //

        glClearColor(0.2f, 0.2f, 0.22f, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //
        // force update of the _proj matrix
        //

        int width = 0;
        int height = 0;
        glfwGetWindowSize(_window, &width, &height);
        onResize(width, height);
    }

    void onResize(int width, int height)
    {
        auto aspect = static_cast<float>(width) / static_cast<float>(height);
        _proj = perspective(radians(FOV_DEGREES), aspect, NEAR_PLANE, FAR_PLANE);
    }

    void onKeyPress(int key, int scancode, int mods)
    {
        if (scancode == glfwGetKeyScancode(GLFW_KEY_SPACE)) {
            // TODO(shamyl@gmail.com): Modulate the isosurface i guess
        }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void onMouseMove(const vec2& pos, const vec2& delta)
    {
        if (_mouseButtonState[0]) {
            // simple x/y trackball
            float trackballSpeed = 0.004 * M_PI;
            mat4 xRot = rotate(mat4(1), -1 * delta.y * trackballSpeed, vec3(1, 0, 0));
            mat4 yRot = rotate(mat4(1), 1 * delta.x * trackballSpeed, vec3(0, 1, 0));
            _model = xRot * yRot * _model;
        }
    }

    void onMouseDown(int button, int mods)
    {
        if (button < 3) {
            _mouseButtonState[button] = true;
        }
    }

    void onMouseUp(int button, int mods)
    {
        if (button < 3) {
            _mouseButtonState[button] = false;
        }
    }

    void onMouseWheel(const vec2& scrollOffset)
    {
        // move camera forward/backward
        float cameraDollySpeed = 0.1F;
        _cameraZPosition += cameraDollySpeed * -scrollOffset.y;
        _cameraZPosition = min(_cameraZPosition, 0.01F);
    }

    void drawFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(_program.program);

        auto view = lookAt(vec3(0, 0, _cameraZPosition), vec3(0, 0, 0), vec3(0, 1, 0));
        auto mvp = _proj * view * _model;

        glUniformMatrix4fv(_program.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        glUniformMatrix4fv(_program.uniformLocModel, 1, GL_FALSE, value_ptr(_model));
        _mcTriangleConsumer.draw();
    }

    void buildMesh()
    {
        auto volume = Volume { vec3(50), 10.0F, 1.0F };
        auto transform = glm::scale(mat4(1), vec3(1 / 20.0F)) * glm::translate(mat4(1), vec3(-50));

        auto start = glfwGetTime();
        mc::march(volume, _mcTriangleConsumer, 0.5F, transform, true);
        auto end = glfwGetTime();
        std::cout << "Marching took " << (end - start) << " seconds" << std::endl;
    }
};

#pragma mark - Main

int main()
{
    OpenGLCubeApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
