//
//  util.h
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef util_h
#define util_h

// Silence Apple's OpenGL depecation warnings
#define GL_SILENCE_DEPRECATION 1

#include <GL/glew.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#include <GLFW/glfw3.h>
#pragma clang diagnosic pop

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

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

}

#endif /* util_h */
