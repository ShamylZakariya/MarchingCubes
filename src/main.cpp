//
//  gl_main.cpp
//  OpenGLCubism
//
//  Created by Shamyl Zakariya on 11/7/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

// this is to silence Apple's OpenGL depecation warnings
#define GL_SILENCE_DEPRECATION 1

#include <GL/glew.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#include <GLFW/glfw3.h>
#pragma clang diagnosic pop

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <chrono>


using namespace glm;

#pragma mark - Constants

const int WIDTH = 800;
const int HEIGHT = 600;
const float NEAR_PLANE = 0.1f;
const float FAR_PLANE = 1000.0f;
const float FOV_DEGREES = 50.0F;

#pragma mark - Helpers

namespace {

std::string ReadFile(const std::string& filename) {
    std::ifstream in(filename.c_str());
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

GLuint LoadTexture(const std::string &filename) {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}

bool CheckGlError(const char* ctx) {
#ifndef NDEBUG
    GLint err = glGetError();
    if ( err != GL_NO_ERROR ) {
        std::cerr << "GL error at " << ctx << " err: " << err << std::endl;
        return true;
    }
    return false;
#endif
    return false;
}

GLuint CreateShader(GLenum shader_type, const char* src) {
    GLuint shader = glCreateShader(shader_type);
    if ( !shader ) {
        CheckGlError("glCreateShader");
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);
    
    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if ( !compiled ) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if ( infoLogLen > 0 ) {
            auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
            if ( infoLog ) {
                glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog);
                std::cerr << "Could not compile " <<
                (shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment") <<
                " error: " << std::string(infoLog) << std::endl;
                
                throw std::runtime_error("Could not compile shader");
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint CreateProgram(const char* vtxSrc, const char* fragSrc) {
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;
    
    vtxShader = CreateShader(GL_VERTEX_SHADER, vtxSrc);
    if ( !vtxShader )
        goto exit;
    
    fragShader = CreateShader(GL_FRAGMENT_SHADER, fragSrc);
    if ( !fragShader )
        goto exit;
    
    program = glCreateProgram();
    if ( !program ) {
        CheckGlError("glCreateProgram");
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);
    
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if ( !linked ) {
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if ( infoLogLen ) {
            auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
            if ( infoLog ) {
                glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog);
                std::cerr << "Could not link program: " << infoLog << std::endl;
                throw std::runtime_error("Could not link program");
            }
        }
        glDeleteProgram(program);
        program = 0;
        std::cerr << "Could not link program" << std::endl;
        throw std::runtime_error("Could not link program");
    }
    
exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}
}


#pragma mark - Data

struct Vertex {
    vec3 pos;
    vec3 color;
    vec2 texCoord;
    
    enum class AttributeLayout : GLuint {
        Pos = 0,
        Color = 1,
        Texcoord = 2,
    };
};


const std::vector<Vertex> Vertices = {
    // -Z
    {{ -1, -1, -1 }, { +1,+1,+0 }, { 0, 0 }},
    {{ +1, +1, -1 }, { +1,+1,+0 }, { 1, 1 }},
    {{ +1, -1, -1 }, { +1,+1,+0 }, { 1, 0 }},

    {{ -1, -1, -1 }, { +1,+1,+0 }, { 0, 0 }},
    {{ -1, +1, -1 }, { +1,+1,+0 }, { 0, 1 }},
    {{ +1, +1, -1 }, { +1,+1,+0 }, { 1, 1 }},

    // +X
    {{ +1, -1, -1 }, { +1,+0,+1 }, { 0, 0 }},
    {{ +1, +1, +1 }, { +1,+0,+1 }, { 1, 1 }},
    {{ +1, -1, +1 }, { +1,+0,+1 }, { 1, 0 }},

    {{ +1, -1, -1 }, { +1,+0,+1 }, { 0, 0 }},
    {{ +1, +1, -1 }, { +1,+0,+1 }, { 0, 1 }},
    {{ +1, +1, +1 }, { +1,+0,+1 }, { 1, 1 }},
    
    // +Z
    {{ +1, -1, +1 }, { +0,+1,+1 }, { 0, 0 }},
    {{ -1, +1, +1 }, { +0,+1,+1 }, { 1, 1 }},
    {{ -1, -1, +1 }, { +0,+1,+1 }, { 1, 0 }},

    {{ +1, -1, +1 }, { +0,+1,+1 }, { 0, 0 }},
    {{ +1, +1, +1 }, { +0,+1,+1 }, { 0, 1 }},
    {{ -1, +1, +1 }, { +0,+1,+1 }, { 1, 1 }},
    
    // -X
    {{ -1, -1, +1 }, { +1,+0,+0 }, { 0, 0 }},
    {{ -1, +1, -1 }, { +1,+0,+0 }, { 1, 1 }},
    {{ -1, -1, -1 }, { +1,+0,+0 }, { 1, 0 }},

    {{ -1, -1, +1 }, { +1,+0,+0 }, { 0, 0 }},
    {{ -1, +1, +1 }, { +1,+0,+0 }, { 0, 1 }},
    {{ -1, +1, -1 }, { +1,+0,+0 }, { 1, 1 }},
    
    // +Y
    {{ -1, +1, -1 }, { +0,+1,+0 }, { 0, 0 }},
    {{ +1, +1, +1 }, { +0,+1,+0 }, { 1, 1 }},
    {{ +1, +1, -1 }, { +0,+1,+0 }, { 1, 0 }},

    {{ -1, +1, -1 }, { +0,+1,+0 }, { 0, 0 }},
    {{ -1, +1, +1 }, { +0,+1,+0 }, { 0, 1 }},
    {{ +1, +1, +1 }, { +0,+1,+0 }, { 1, 1 }},
    
    // -Y
    {{ -1, -1, -1 }, { +0,+0,+1 }, { 0, 0 }},
    {{ +1, -1, -1 }, { +0,+0,+1 }, { 0, 1 }},
    {{ +1, -1, +1 }, { +0,+0,+1 }, { 1, 1 }},

    {{ -1, -1, -1 }, { +0,+0,+1 }, { 0, 0 }},
    {{ +1, -1, +1 }, { +0,+0,+1 }, { 1, 1 }},
    {{ -1, -1, +1 }, { +0,+0,+1 }, { 1, 0 }},
};

#pragma mark -

class OpenGLCubeApplication {
public:
    
    void run() {
        initWindow();
        initGl();

        while (!glfwWindowShouldClose(_window)) {
            glfwPollEvents();
            drawFrame();
            glfwSwapBuffers(_window);
        }
    }
    
    ~OpenGLCubeApplication() {
        if ( _vboState > 0 ) glDeleteVertexArrays(1, &_vboState);
        if ( _vertexVboId > 0 ) glDeleteBuffers(1, &_vertexVboId);
        if ( _textureId > 0 ) glDeleteTextures(1, &_textureId);
    }
    
private:
    
    GLFWwindow* _window;

    GLuint _textureId = 0;
    GLuint _vertexVboId = 0;
    GLuint _vboState = 0;
    GLuint _program = 0;
    GLint _uniformLocMVP = -1;
    GLint _uniformLocTexSampler = -1;
    
    mat4 _model = mat4(1);
    mat4 _view = mat4(1);
    mat4 _proj = mat4(1);

private:
    
    void initWindow() {
        glfwInit();
        
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        _window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL", nullptr, nullptr);
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height){
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onResize(width, height);
        });

        glfwMakeContextCurrent(_window);
    }
        
    void initGl() {
        
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
        auto vertSrc = ReadFile(vertFile);
        auto fragSrc = ReadFile(fragFile);
        _program = CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        CheckGlError("Loading program");
        
        _uniformLocMVP = glGetUniformLocation(_program, "uMVP");
        _uniformLocTexSampler = glGetUniformLocation(_program, "uTexSampler");
        CheckGlError("Looking up uniforms");

        //
        // load texture
        //

        _textureId = LoadTexture("textures/color.png");
                
        //
        // create VBOs
        //

        glGenBuffers(1, &_vertexVboId);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * Vertices.size(),
                     Vertices.data(),
                     GL_STATIC_DRAW);
        
        CheckGlError("Creating _vertexVboId");
        
        glGenVertexArrays(1, &_vboState);
        glBindVertexArray(_vboState);
        CheckGlError("Creating _vboState");
        
        glVertexAttribPointer(static_cast<GLuint>(Vertex::AttributeLayout::Pos), 3, GL_FLOAT,
                              GL_FALSE,
                              sizeof(Vertex),
                              (const GLvoid*)offsetof(Vertex, pos));
        
        glVertexAttribPointer(static_cast<GLuint>(Vertex::AttributeLayout::Color), 3, GL_FLOAT,
                              GL_FALSE,
                              sizeof(Vertex),
                              (const GLvoid*)offsetof(Vertex, color));
        
        glVertexAttribPointer(static_cast<GLuint>(Vertex::AttributeLayout::Texcoord), 2, GL_FLOAT,
                              GL_FALSE,
                              sizeof(Vertex),
                              (const GLvoid*)offsetof(Vertex, texCoord));
                
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Pos));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Texcoord));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Color));
        CheckGlError("Configuring vertex attrib pointers");
                
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
    
    void onResize(int width, int height) {
        auto aspect = static_cast<float>(width) / static_cast<float>(height);
        _proj = perspective(radians(FOV_DEGREES), aspect, NEAR_PLANE, FAR_PLANE);
    }
    
    void drawFrame() {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float elapsedSeconds = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        //
        //  Position cube at origin, spinning a bit
        //
        const float xAngle = elapsedSeconds * M_PI * 0.1F;
        const float yAngle = elapsedSeconds * M_PI * 0.3F;
        const float zAngle = elapsedSeconds * M_PI * -0.2F;
        const vec3 cubePos(0,0,0);
        _model = rotate(rotate(rotate(translate(mat4(1),cubePos), zAngle, vec3(0,0,1)), yAngle, vec3(0,1,0)), xAngle, vec3(1,0,0));

        //
        //  Make camera look at cube
        //

        _view = lookAt(vec3(0,1,-4), cubePos, vec3(0,1,0));
        const mat4 mvp = _proj * _view * _model;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(_program);
        glUniformMatrix4fv(_uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glUniform1i(_uniformLocTexSampler, 0);
        
        glBindVertexArray(_vboState);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(Vertices.size()));
    }
};

#pragma mark - Main

int main() {
    OpenGLCubeApplication app;
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
