//
//  main.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

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
    GLint uniformLocWireframe = -1;

    ~ProgramState()
    {
        if (program > 0) {
            glDeleteProgram(program);
        }
    }

    bool build(const std::string& vertSrc, const std::string& fragSrc)
    {
        program = util::CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        uniformLocModel = glGetUniformLocation(program, "uModel");
        uniformLocWireframe = glGetUniformLocation(program, "uWireframe");

        // we're not using uniformLocModel (yet) so it is -1 here
        return uniformLocMVP >= 0 && uniformLocWireframe >= 0; // && uniformLocModel >= 0;
    }
};

#pragma mark -

#pragma mark -

class OpenGLCubeApplication {
public:
    OpenGLCubeApplication()
    {
        initWindow();
        initGl();
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
    TriangleConsumer _rawTriangleConsumer;
    IndexedTriangleConsumer _indexedTriangleConsumer { IndexedTriangleConsumer::Strategy::Basic };
    int _wedges = 4;
    bool _wireframe = true;

    mat4 _proj = mat4(1);

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
        if (!_program.build(vertSrc, fragSrc)) {
            throw std::runtime_error("Unable to build program");
        }

        buildTriangleData(_rawTriangleConsumer, _wedges);
        buildTriangleData(_indexedTriangleConsumer, _wedges);

        //
        // some constant GL state
        //

        glClearColor(0.2f, 0.2f, 0.22f, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);
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
            ++_wedges;
            buildTriangleData(_rawTriangleConsumer, _wedges);
            buildTriangleData(_indexedTriangleConsumer, _wedges);
        } else if (scancode == glfwGetKeyScancode(GLFW_KEY_W)) {
            _wireframe = !_wireframe;
        }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void drawFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float elapsedSeconds = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        auto view = lookAt(vec3(0, 1, -4), vec3(0, 0, 0), vec3(0, 1, 0));
        auto model = translate(mat4(1), vec3(-1.2, 0, 0));
        auto mvp = _proj * view * model;

        glUseProgram(_program.program);
        glUniform1f(_program.uniformLocWireframe, _wireframe ? 1 : 0);
        
        glUniformMatrix4fv(_program.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        _rawTriangleConsumer.draw();

        model = translate(mat4(1), vec3(+1.2, 0, 0));
        mvp = _proj * view * model;
        glUniformMatrix4fv(_program.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        _indexedTriangleConsumer.draw();
    }

    void buildTriangleData(ITriangleConsumer& consumer, int wedges)
    {
        Vertex center = { { 0, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } };
        float wedgeAngle = static_cast<float>(M_PI * 2 / wedges);
        float angle = 0;
        float radius = 1;
        float hue = 0;
        float hueIncrement = 360.0F / static_cast<float>(wedges - 1);

        consumer.start();

        for (int i = 0; i < wedges; i++) {

            vec3 a = vec3(cos(angle), sin(angle), 0) * radius;
            vec3 aColor = static_cast<vec3>(util::color::Hsv2Rgb(util::color::hsv { hue, 0.8F, 0.8F }));

            angle += wedgeAngle;
            hue += hueIncrement;

            vec3 b = vec3(cos(angle), sin(angle), 0) * radius;
            vec3 bColor = static_cast<vec3>(util::color::Hsv2Rgb(util::color::hsv { hue, 0.8F, 0.8F }));
            Vertex av = { a, aColor, { 0, 0, 1 } };
            Vertex bv = { b, bColor, { 0, 0, 1 } };

            consumer.addTriangle(Triangle { center, av, bv });
        }

        consumer.finish();
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
