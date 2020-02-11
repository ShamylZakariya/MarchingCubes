//
//  io.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_io_h
#define mc_io_h

#include <array>
#include <limits>
#include <string>

#include <epoxy/gl.h>

namespace mc {
namespace util {

    /**
    Read a file into a string
    */
    std::string ReadFile(const std::string& filename);

    /**
    Loads image into a texture 2d
    */
    GLuint LoadTexture2D(const std::string& filename);

    /**
    Loads images into a cubemap texture in this order:
    0: GL_TEXTURE_CUBE_MAP_POSITIVE_X 	Right
    1: GL_TEXTURE_CUBE_MAP_NEGATIVE_X 	Left
    2: GL_TEXTURE_CUBE_MAP_POSITIVE_Y 	Top
    3: GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 	Bottom
    4: GL_TEXTURE_CUBE_MAP_POSITIVE_Z 	Back
    5: GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 	Front
    */
    GLuint LoadTextureCube(const std::array<std::string,6> &faces);

    GLuint LoadTextureCube(const std::string &folder, const std::string &ext = ".jpg");

    /**
    Checks for GL error, throwing if there is one
    */
    void CheckGlError(const char* ctx);

    /**
    Creates a shader of specified type from provided source
    */
    GLuint CreateShader(GLenum shader_type, const char* src, const char* filename = nullptr);

    /**
    Creates full shader program from vertex and fragment sources
    */
    GLuint CreateProgram(const char* vtxSrc, const char* fragSrc);

    /**
     * Create full shader program from files
     */
    GLuint CreateProgramFromFiles(const char* vtxFile, const char* fragFile);

}
} // namespace mc::util

#endif /* util_h */
