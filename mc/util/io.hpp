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
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>

#include <epoxy/gl.h>

namespace mc {
namespace util {

    /**
    Read a file into a string
    */
    std::string ReadFile(const std::string& filename);

    class Image {
    private:
        unsigned char* _bytes = nullptr;
        int _width = 0;
        int _height = 0;
        int _channels = 0;

    public:
        Image() = delete;
        explicit Image(const std::string& filename);
        Image(const Image&) = delete;
        Image(Image&&) = delete;
        Image& operator=(const Image&) = delete;

        ~Image();

        const unsigned char* getBytes() const { return _bytes; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        int getChannels() const { return _height; }
    };

    class TextureHandle {
    private:
        GLuint _id;
        GLenum _target;
        int _width;
        int _height;
        int _mipLevels;

    public:
        TextureHandle(GLuint id, GLenum target, int width, int height)
            : _id(id)
            , _target(target)
            , _width(width)
            , _height(height)
        {
            _mipLevels = static_cast<int>(std::log2(std::min(_width, _height)));
        }

        ~TextureHandle()
        {
            glDeleteTextures(1, &_id);
        }

        GLuint getId() const { return _id; }
        GLenum getTarget() const { return _target; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        int getMipLevels() const { return _mipLevels; }
    };

    typedef std::shared_ptr<TextureHandle> TextureHandleRef;

    /**
    Loads image into a texture 2d
    */
    TextureHandleRef LoadTexture2D(const std::string& filename, std::function<void()> setup = nullptr);

    /**
    Loads images into a cubemap texture in this order:
    0: GL_TEXTURE_CUBE_MAP_POSITIVE_X 	Right
    1: GL_TEXTURE_CUBE_MAP_NEGATIVE_X 	Left
    2: GL_TEXTURE_CUBE_MAP_POSITIVE_Y 	Top
    3: GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 	Bottom
    4: GL_TEXTURE_CUBE_MAP_POSITIVE_Z 	Back
    5: GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 	Front
    */
    TextureHandleRef LoadTextureCube(const std::array<std::string, 6>& faces);

    /**
     * Helper function to load a skybox from a folder, where the faces are named
     * [right, left, top, bottom, front, back]
     */
    TextureHandleRef LoadTextureCube(const std::string& folder, const std::string& ext = ".jpg");

    /**
    Checks for GL error, throwing if there is one. Note: If you want a version
    which is compiled away for release builds see the CHECK_GL_ERROR macro.
    */
    void CheckGlError(const char* ctx);

    /**
    Creates a shader of specified type from provided source
    */
    GLuint CreateShader(GLenum shader_type, const char* src, std::function<void(const std::string&)> onError);

    /**
    Creates full shader program from vertex and fragment sources
    */
    GLuint CreateProgram(const char* vtxSrc, const char* fragSrc,
        std::function<void(const std::string&)> onVertexError,
        std::function<void(const std::string&)> onFragmentError);

    /**
     * Create full shader program from a file which has a "vertex:" section and "fragment:" section
     */
    GLuint CreateProgramFromFile(const char* glslFile, const std::map<std::string, std::string>& substitutions = {});
}
} // namespace mc::util

#ifndef NDEBUG
#define CHECK_GL_ERROR(ctx) mc::util::CheckGlError((ctx))
#else
#define CHECK_GL_ERROR(ctx) ;
#endif

#endif /* util_h */
