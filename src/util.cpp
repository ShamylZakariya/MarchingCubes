//
//  util.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include "util.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

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

namespace color {

    hsv Rgb2Hsv(rgb in)
    {
        hsv out;
        float min, max, delta;

        min = in.r < in.g ? in.r : in.g;
        min = min < in.b ? min : in.b;

        max = in.r > in.g ? in.r : in.g;
        max = max > in.b ? max : in.b;

        out.v = max; // v
        delta = max - min;
        if (delta < 0.00001F) {
            out.s = 0;
            out.h = 0; // undefined, maybe nan?
            return out;
        }
        if (max > 0.0) { // NOTE: if Max is == 0, this divide would cause a crash
            out.s = (delta / max); // s
        } else {
            // if max is 0, then r = g = b = 0
            // s = 0, h is undefined
            out.s = 0.0;
            out.h = NAN; // its now undefined
            return out;
        }
        if (in.r >= max) // > is bogus, just keeps compilor happy
            out.h = (in.g - in.b) / delta; // between yellow & magenta
        else if (in.g >= max)
            out.h = 2.0F + (in.b - in.r) / delta; // between cyan & yellow
        else
            out.h = 4.0F + (in.r - in.g) / delta; // between magenta & cyan

        out.h *= 60.0F; // degrees

        if (out.h < 0.0)
            out.h += 360.0F;

        return out;
    }

    rgb Hsv2Rgb(hsv in)
    {
        float hh, p, q, t, ff;
        long i;
        rgb out;

        if (in.s <= 0.0) { // < is bogus, just shuts up warnings
            out.r = in.v;
            out.g = in.v;
            out.b = in.v;
            return out;
        }
        hh = in.h;
        if (hh >= 360.0F)
            hh = 0.0;
        hh /= 60.0F;
        i = (long)hh;
        ff = hh - i;
        p = in.v * (1.0F - in.s);
        q = in.v * (1.0F - (in.s * ff));
        t = in.v * (1.0F - (in.s * (1.0F - ff)));

        switch (i) {
        case 0:
            out.r = in.v;
            out.g = t;
            out.b = p;
            break;
        case 1:
            out.r = q;
            out.g = in.v;
            out.b = p;
            break;
        case 2:
            out.r = p;
            out.g = in.v;
            out.b = t;
            break;

        case 3:
            out.r = p;
            out.g = q;
            out.b = in.v;
            break;
        case 4:
            out.r = t;
            out.g = p;
            out.b = in.v;
            break;
        case 5:
        default:
            out.r = in.v;
            out.g = p;
            out.b = q;
            break;
        }
        return out;
    }

}

}
