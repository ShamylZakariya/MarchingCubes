//
//  io.hpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_io_h
#define mc_io_h

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
} // namespace mc::util

#endif /* util_h */
