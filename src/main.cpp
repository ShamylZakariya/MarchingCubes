//
//  main.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "util.h"

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

struct Vertex {
    vec3 pos;
    vec3 color;

    enum class AttributeLayout : GLuint {
        Pos = 0,
        Color = 1
    };

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color;
    }
};

namespace std {
template <>
struct hash<Vertex> {
    size_t operator()(Vertex const& vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1);
    }
};
}

struct ProgramState {
    GLuint program = 0;
    GLint uniformLocMVP = -1;

    bool build(const std::string& vertSrc, const std::string& fragSrc)
    {
        program = util::CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        if (util::CheckGlError("CreateProgram")) {
            return false;
        }
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        return uniformLocMVP >= 0;
    }
};

const std::vector<Vertex> Vertices = {
    // -Z
    { { -1, -1, -1 }, { +1, +1, +0 } },
    { { +1, +1, -1 }, { +1, +1, +0 } },
    { { +1, -1, -1 }, { +1, +1, +0 } },

    { { -1, -1, -1 }, { +1, +1, +0 } },
    { { -1, +1, -1 }, { +1, +1, +0 } },
    { { +1, +1, -1 }, { +1, +1, +0 } },

    // +X
    { { +1, -1, -1 }, { +1, +0, +1 } },
    { { +1, +1, +1 }, { +1, +0, +1 } },
    { { +1, -1, +1 }, { +1, +0, +1 } },

    { { +1, -1, -1 }, { +1, +0, +1 } },
    { { +1, +1, -1 }, { +1, +0, +1 } },
    { { +1, +1, +1 }, { +1, +0, +1 } },

    // +Z
    { { +1, -1, +1 }, { +0, +1, +1 } },
    { { -1, +1, +1 }, { +0, +1, +1 } },
    { { -1, -1, +1 }, { +0, +1, +1 } },

    { { +1, -1, +1 }, { +0, +1, +1 } },
    { { +1, +1, +1 }, { +0, +1, +1 } },
    { { -1, +1, +1 }, { +0, +1, +1 } },

    // -X
    { { -1, -1, +1 }, { +1, +0, +0 } },
    { { -1, +1, -1 }, { +1, +0, +0 } },
    { { -1, -1, -1 }, { +1, +0, +0 } },

    { { -1, -1, +1 }, { +1, +0, +0 } },
    { { -1, +1, +1 }, { +1, +0, +0 } },
    { { -1, +1, -1 }, { +1, +0, +0 } },

    // +Y
    { { -1, +1, -1 }, { +0, +1, +0 } },
    { { +1, +1, +1 }, { +0, +1, +0 } },
    { { +1, +1, -1 }, { +0, +1, +0 } },

    { { -1, +1, -1 }, { +0, +1, +0 } },
    { { -1, +1, +1 }, { +0, +1, +0 } },
    { { +1, +1, +1 }, { +0, +1, +0 } },

    // -Y
    { { -1, -1, -1 }, { +0, +0, +1 } },
    { { +1, -1, -1 }, { +0, +0, +1 } },
    { { +1, -1, +1 }, { +0, +0, +1 } },

    { { -1, -1, -1 }, { +0, +0, +1 } },
    { { +1, -1, +1 }, { +0, +0, +1 } },
    { { -1, -1, +1 }, { +0, +0, +1 } },
};

#pragma mark -

class OpenGLCubeApplication {
public:
    void run()
    {
        initWindow();
        initGl();

        while (!glfwWindowShouldClose(_window)) {
            glfwPollEvents();
            drawFrame();
            glfwSwapBuffers(_window);
        }
    }

    ~OpenGLCubeApplication()
    {
        if (_vboState > 0)
            glDeleteVertexArrays(1, &_vboState);
        if (_vertexVboId > 0)
            glDeleteBuffers(1, &_vertexVboId);
    }

private:
    GLFWwindow* _window;

    GLuint _vertexVboId = 0;
    GLuint _vboState = 0;
    ProgramState _program;

    mat4 _model = mat4(1);
    mat4 _view = mat4(1);
    mat4 _proj = mat4(1);

private:
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        _window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL", nullptr, nullptr);
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onResize(width, height);
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

        //
        // create VBOs
        //

        glGenBuffers(1, &_vertexVboId);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * Vertices.size(),
            Vertices.data(),
            GL_STATIC_DRAW);

        util::CheckGlError("Creating _vertexVboId");

        glGenVertexArrays(1, &_vboState);
        glBindVertexArray(_vboState);
        util::CheckGlError("Creating _vboState");

        glVertexAttribPointer(static_cast<GLuint>(Vertex::AttributeLayout::Pos), 3, GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, pos));

        glVertexAttribPointer(static_cast<GLuint>(Vertex::AttributeLayout::Color), 3, GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, color));

        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Pos));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Color));
        util::CheckGlError("Configuring vertex attrib pointers");

        glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
        glBindVertexArray(0);

        //
        // some constant GL state
        //

        glClearColor(0.2f, 0.2f, 0.22f, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);

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

    void drawFrame()
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float elapsedSeconds = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        //
        //  Position cube at origin, spinning a bit
        //
        const float xAngle = elapsedSeconds * M_PI * 0.1F;
        const float yAngle = elapsedSeconds * M_PI * 0.3F;
        const float zAngle = elapsedSeconds * M_PI * -0.2F;
        const vec3 cubePos(0, 0, 0);
        _model = rotate(rotate(rotate(translate(mat4(1), cubePos), zAngle, vec3(0, 0, 1)), yAngle, vec3(0, 1, 0)), xAngle, vec3(1, 0, 0));

        //
        //  Make camera look at cube
        //

        _view = lookAt(vec3(0, 1, -4), cubePos, vec3(0, 1, 0));
        const mat4 mvp = _proj * _view * _model;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(_program.program);
        glUniformMatrix4fv(_program.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));

        glBindVertexArray(_vboState);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(Vertices.size()));
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
