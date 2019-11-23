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

    ~ProgramState()
    {
        if (program > 0) {
            glDeleteProgram(program);
        }
    }

    bool build(const std::string& vertSrc, const std::string& fragSrc)
    {
        program = util::CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        util::CheckGlError("CreateProgram");
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        return uniformLocMVP >= 0;
    }
};

#pragma mark -

class IndexedVertexStorage {
private:
    GLuint _vertexVboId = 0;
    GLuint _indexVboId = 0;
    GLuint _vboState = 0;
    std::size_t _numIndices = 0;
    std::size_t _numVertices = 0;
    float _growthFactor;
    bool _vboStateUpdateDeferred = false;

public:
    IndexedVertexStorage(float growthFactor = 1.5F)
        : _growthFactor(growthFactor)
    {
    }

    IndexedVertexStorage(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices, float growthFactor = 1.5F)
        : _growthFactor(growthFactor)
    {
        write(vertices, indices);
    }

    ~IndexedVertexStorage()
    {
        if (_vboState > 0)
            glDeleteVertexArrays(1, &_vboState);
        if (_vertexVboId > 0)
            glDeleteBuffers(1, &_vertexVboId);
        if (_indexVboId > 0)
            glDeleteBuffers(1, &_indexVboId);
    }

    void draw()
    {
        glBindVertexArray(_vboState);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_numIndices), GL_UNSIGNED_INT, nullptr);
    }

    void write(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices)
    {
        _vboStateUpdateDeferred = true;
        writeVertices(vertices);
        writeIndices(indices);
        _vboStateUpdateDeferred = false;
        createVboState();
    }

    void writeVertices(const std::vector<Vertex>& vertices)
    {
        if (vertices.size() > _numVertices) {
            // resize GPU storage

            auto storageSize = static_cast<std::size_t>(vertices.size() * _growthFactor);
            std::cout << "IndexedVertexStorage::writeVertices resizing Vertex store from " << _numVertices << " to " << storageSize << std::endl;
            _numVertices = vertices.size();

            if (_vertexVboId > 0)
                glDeleteBuffers(1, &_vertexVboId);

            glGenBuffers(1, &_vertexVboId);
            glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
            glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(Vertex) * storageSize,
                vertices.data(),
                GL_STREAM_DRAW);

            util::CheckGlError("Updating new _vertexVboId");

            if (!_vboStateUpdateDeferred) {
                createVboState();

                // TODO: Can we just update _vboState like this?
                //glBindVertexArray(_vboState);
                //glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
                //glBindVertexArray(0);
            }

            util::CheckGlError("Updating _vboState with new _vertexVboId");
        } else {
            // GPU storage suffices, just copy data over
            _numVertices = vertices.size();

            // map
            glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
            auto* data = static_cast<Vertex*>(glMapBufferRange(
                GL_ARRAY_BUFFER,
                0, _numVertices * sizeof(Vertex),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

            // write
            memcpy(data, vertices.data(), _numVertices * sizeof(Vertex));

            // unmap
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
    }

    void writeIndices(const std::vector<GLuint>& indices)
    {
        if (indices.size() > _numIndices) {
            // GPU storage

            auto storageSize = static_cast<std::size_t>(indices.size() * _growthFactor);
            std::cout << "IndexedVertexStorage::writeIndices resizing Index store from " << _numIndices << " to " << storageSize << std::endl;
            _numIndices = indices.size();

            if (_indexVboId > 0)
                glDeleteBuffers(1, &_indexVboId);

            glGenBuffers(1, &_indexVboId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                sizeof(GLuint) * storageSize,
                indices.data(),
                GL_STREAM_DRAW);

            util::CheckGlError("Creating new _indexVboId");

            if (!_vboStateUpdateDeferred) {
                // TODO: Can we just update _vboState like this?
                createVboState();
                
                //glBindVertexArray(_vboState);
                //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);
                //glBindVertexArray(0);
            }

            util::CheckGlError("Updating _vboState with new _vertexVboId");
        } else {
            // GPU storage suffices, just copy data over
            _numIndices = indices.size();

            // map
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);
            auto* data = static_cast<GLuint*>(glMapBufferRange(
                GL_ELEMENT_ARRAY_BUFFER,
                0, _numIndices * sizeof(GLuint),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

            // write
            memcpy(data, indices.data(), _numIndices * sizeof(GLuint));

            // unmap
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
    }

private:
    void createVboState()
    {
        if (_vboState) {
            glDeleteVertexArrays(1, &_vboState);
        }

        glGenVertexArrays(1, &_vboState);
        glBindVertexArray(_vboState);
        util::CheckGlError("Creating _vboState");

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Pos),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, pos));

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Color),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, color));

        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Pos));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Color));
        util::CheckGlError("Configuring vertex attrib pointers");

        glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexVboId);

        glBindVertexArray(0);
    }
};

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
    std::shared_ptr<IndexedVertexStorage> _storage;

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

        buildGeometry();

        //
        // some constant GL state
        //

        glClearColor(0.2f, 0.2f, 0.22f, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);

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
        //  Position target at origin, spinning a bit
        //

        const float xAngle = elapsedSeconds * M_PI * 0;
        const float yAngle = elapsedSeconds * M_PI * 0;
        const float zAngle = elapsedSeconds * M_PI * 0;
        const vec3 target(0, 0, 0);
        _model = rotate(rotate(rotate(translate(mat4(1), target), zAngle, vec3(0, 0, 1)), yAngle, vec3(0, 1, 0)), xAngle, vec3(1, 0, 0));

        //
        //  Make camera look at cube
        //

        _view = lookAt(vec3(0, 1, -4), target, vec3(0, 1, 0));
        const mat4 mvp = _proj * _view * _model;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(_program.program);
        glUniformMatrix4fv(_program.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));

        _storage->draw();
    }

    void buildGeometry()
    {
        auto vertices = std::vector<Vertex> {
            { { -0.5f, -0.5f, 0 }, { 1.0f, 0.0f, 0.0f } },
            { { 0.5f, -0.5f, 0 }, { 0.0f, 1.0f, 0.0f } },
            { { 0.5f, 0.5f, 0 }, { 0.0f, 0.0f, 1.0f } },
            { { -0.5f, 0.5f, 0 }, { 1.0f, 1.0f, 1.0f } }
        };

        auto indices = std::vector<GLuint> {
            0, 1, 2, 2, 3, 0
        };

        _storage = std::make_shared<IndexedVertexStorage>(vertices, indices);
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
