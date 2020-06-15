#ifndef cubemap_blur_hpp
#define cubemap_blur_hpp

#include <epoxy/gl.h>

#include <mc/util/io.hpp>

/**
 * Blurs a cubemap texture, returning a new blurred version.
 * srcCubemap: The cubemap texture to blue.
 * blurHalfArcWidth: The half-arc width in radians of the blur lookup.
 * size: The size of the destination texture into which to render the result.
 */
std::unique_ptr<mc::util::TextureHandle> BlurCubemap(mc::util::TextureHandleRef srcCubemap, float blurHalfArcWidth, int size);

#endif