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

    TextureHandleRef LoadTexture2D(const std::string& filename)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* data = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!data) {
            throw std::runtime_error("failed to load texture image!");
        }

        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        return std::make_shared<TextureHandle>(textureId, GL_TEXTURE_2D, texWidth, texHeight);
    }

    TextureHandleRef LoadTextureCube(const std::array<std::string, 6>& faces)
    {
        unsigned int textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++) {
            stbi_uc* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            } else {
                std::cout << "Unable to load cubemap face[" << i << "] path(" << faces[i] << ")" << std::endl;
                stbi_image_free(data);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // note, sampling mipmap lods requires glEnable(GL_TEXTURE_CUBEMAP_SEAMLESS)
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        return std::make_shared<TextureHandle>(textureId, GL_TEXTURE_CUBE_MAP, width, height);
    }

    TextureHandleRef LoadTextureCube(const std::string& folder, const std::string& ext)
    {
        return LoadTextureCube({
            folder + "/right" + ext,
            folder + "/left" + ext,
            folder + "/top" + ext,
            folder + "/bottom" + ext,
            folder + "/front" + ext,
            folder + "/back" + ext,
        });
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

    void CheckGlError(const std::string &ctx) {
#ifndef NDEBUG
        CheckGlError(ctx.c_str());
#endif
    }


    GLuint CreateShader(GLenum shader_type, const char* src, const char* filename)
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
                    if (filename) {
                        std::cerr << "Could not compile "
                                  << (shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                                  << " file (" << filename << ")\t error: "
                                  << std::string(infoLog) << std::endl;
                    } else {
                        std::cerr << "Could not compile "
                                  << (shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                                  << " error: " << std::string(infoLog) << std::endl;
                    }

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

    GLuint CreateProgramFromFiles(const char* vtxFile, const char* fragFile)
    {
        auto vertSrc = ReadFile(vtxFile);
        if (vertSrc.length() == 0) {
            std::cerr << "Unable to read vertex file: " << vtxFile << std::endl;
            return 0;
        }

        auto fragSrc = ReadFile(fragFile);
        if (fragSrc.length() == 0) {
            std::cerr << "Unable to read fragment file: " << fragFile << std::endl;
            return 0;
        }

        GLuint vtxShader = 0;
        GLuint fragShader = 0;
        GLuint program = 0;
        GLint linked = GL_FALSE;

        vtxShader = CreateShader(GL_VERTEX_SHADER, vertSrc.c_str(), vtxFile);
        if (!vtxShader)
            goto exit;

        fragShader = CreateShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), fragFile);
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