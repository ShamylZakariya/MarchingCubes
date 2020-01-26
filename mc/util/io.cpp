#include "io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace mc {
namespace util {

    std::string ReadFile(const std::string& filename)
    {
        std::ifstream in(filename.c_str());
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    GLuint LoadTexture(const std::string& filename)
    {
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

    void CheckGlError(const char* ctx)
    {
#ifndef NDEBUG
        GLint err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "GL error at " << ctx << " err: " << err << std::endl;
            throw std::runtime_error("CheckGLError");
        }
#endif
    }

    GLuint CreateShader(GLenum shader_type, const char* src)
    {
        GLuint shader = glCreateShader(shader_type);
        if (!shader) {
            CheckGlError("glCreateShader");
            return 0;
        }
        glShaderSource(shader, 1, &src, nullptr);

        GLint compiled = GL_FALSE;
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLogLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
            if (infoLogLen > 0) {
                auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
                if (infoLog) {
                    glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog);
                    std::cerr << "Could not compile " << (shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " error: " << std::string(infoLog) << std::endl;

                    throw std::runtime_error("Could not compile shader");
                }
            }
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    GLuint CreateProgram(const char* vtxSrc, const char* fragSrc)
    {
        GLuint vtxShader = 0;
        GLuint fragShader = 0;
        GLuint program = 0;
        GLint linked = GL_FALSE;

        vtxShader = CreateShader(GL_VERTEX_SHADER, vtxSrc);
        if (!vtxShader)
            goto exit;

        fragShader = CreateShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!fragShader)
            goto exit;

        program = glCreateProgram();
        if (!program) {
            CheckGlError("glCreateProgram");
            goto exit;
        }
        glAttachShader(program, vtxShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint infoLogLen = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
            if (infoLogLen) {
                auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
                if (infoLog) {
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
} // namespace mc::util