#ifndef cubemap_blur_hpp
#define cubemap_blur_hpp

#include <epoxy/gl.h>

#include <mc/util/io.hpp>

mc::util::TextureHandleRef BlurCubemap(mc::util::TextureHandleRef srcCubemap, float blurHalfArcWidth, int size);

#endif