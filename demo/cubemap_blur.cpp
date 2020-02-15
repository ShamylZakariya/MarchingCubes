#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <mc/triangle_soup.hpp>

#include "cubemap_blur.hpp"

using namespace glm;

namespace {

GLuint CreateDestinationCubemapTexture(int size)
{
    GLuint cubemap = 0;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Allocate space for each side of the cube map
    for (GLuint i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, size,
            size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    return cubemap;
}

std::unique_ptr<mc::util::TextureHandle> CreateGaussianKernel(int size)
{
    std::vector<float> float_data;
    float_data.resize(size * size);

    int halfSize = size / 2;
    float sum = 0;
    for(int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            auto offset = vec2(x - halfSize, y - halfSize);
            auto mag = length(offset);
            auto val = 1.0F - min<float>(mag / halfSize, 1);
            float_data[y * size + x] = val;
            sum += val;
        }
    }

    // normalize kernel
    float normalizingScale = 1 / sum;
    for(auto &v : float_data) {
        v *= normalizingScale;
    }

    // create float 2d texture to hold kernel
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, float_data.data());
    mc::util::CheckGlError("[CreateGaussianKernel] - glTexImage2D");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mc::util::CheckGlError("[CreateGaussianKernel] - glTexParam");
    glBindTexture(GL_TEXTURE_2D, 0);

    mc::util::CheckGlError("[CreateGaussianKernel] - Done");
    return std::make_unique<mc::util::TextureHandle>(textureId, GL_TEXTURE_2D);
}

struct BlurMaterial {
private:
    GLuint _program = 0;
    GLint _uProjectionInverse = -1;
    GLint _uModelViewInverse = -1;
    GLint _uSrcCubemapSampler = -1;
    GLint _uKernelSampler;
    GLint _uBlurHalfArcWidth = -1;
    GLint _uLookX = -1;
    GLint _uLookY = -1;
    GLint _uLookZ = -1;

    mc::util::TextureHandleRef _srcCubemap;
    mc::util::TextureHandleRef _kernel;
    float _blurHalfArcWidth;

public:
    BlurMaterial(mc::util::TextureHandleRef src, mc::util::TextureHandleRef kernel, float blurHalfArcWidth)
        : _srcCubemap(src)
        , _kernel(kernel)
        , _blurHalfArcWidth(blurHalfArcWidth)
    {
        using namespace mc::util;
        _program = CreateProgramFromFiles("shaders/gl/cubemap_blur_vert.glsl", "shaders/gl/cubemap_blur_frag.glsl");
        _uProjectionInverse = glGetUniformLocation(_program, "uProjectionInverse");
        _uModelViewInverse = glGetUniformLocation(_program, "uModelViewInverse");
        _uSrcCubemapSampler = glGetUniformLocation(_program, "uSrcCubemapSampler");
        _uKernelSampler = glGetUniformLocation(_program, "uKernelSampler");
        _uBlurHalfArcWidth = glGetUniformLocation(_program, "uBlurHalfArcWidth");
        _uLookX = glGetUniformLocation(_program, "uLookX");
        _uLookY = glGetUniformLocation(_program, "uLookY");
        _uLookZ = glGetUniformLocation(_program, "uLookZ");
    }

    BlurMaterial(const BlurMaterial& other) = delete;
    BlurMaterial(const BlurMaterial&& other) = delete;

    ~BlurMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void bind(const mat4& projection, const mat4& model, const mat4& view)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _srcCubemap->id());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _kernel->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uProjectionInverse, 1, GL_FALSE, value_ptr(inverse(projection)));
        glUniformMatrix4fv(_uModelViewInverse, 1, GL_FALSE, value_ptr(inverse(view * model)));
        glUniform1i(_uSrcCubemapSampler, 0);
        glUniform1i(_uKernelSampler, 1);

        auto lookX = glm::vec3 { view[0][0], view[1][0], view[2][0] };
        auto lookY = glm::vec3 { view[0][1], view[1][1], view[2][1] };
        auto lookZ = glm::vec3 { view[0][2], view[1][2], view[2][2] };
        glUniform3fv(_uLookX, 1, value_ptr(lookX));
        glUniform3fv(_uLookY, 1, value_ptr(lookY));
        glUniform3fv(_uLookZ, 1, value_ptr(lookZ));
        glUniform1f(_uBlurHalfArcWidth, _blurHalfArcWidth);
    }
};

}

std::unique_ptr<mc::util::TextureHandle>
BlurCubemap(mc::util::TextureHandleRef srcCubemap, float blurHalfArcWidth, int size)
{
    using mc::util::CheckGlError;

    GLuint framebuffer = 0;
    GLuint destCubemapTexId = CreateDestinationCubemapTexture(size);

    // Create framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Attach only the +X cubemap texture (for completeness)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, destCubemapTexId, 0);
    CheckGlError("[BlurCubemap] Set framebuffer texture attachment");

    // Check if current configuration of framebuffer is correct
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        CheckGlError("[BlurCubemap] Framebuffer status");
        std::cout << "Framebuffer not complete!" << std::endl;
    }

    const vec3 lookAts[6] = {
        vec3 { 1.0f, 0.0f, 0.0f },
        vec3 { -1.0f, 0.0f, 0.0f },
        vec3 { 0.0f, 1.0f, 0.0f },
        vec3 { 0.0f, -1.0f, 0.0f },
        vec3 { 0.0f, 0.0f, 1.0f },
        vec3 { 0.0f, 0.0f, -1.0f }
    };
    const vec3 lookUps[6] = {
        vec3 { 0.0f, -1.0f, 0.0f },
        vec3 { 0.0f, -1.0f, 0.0f },
        vec3 { 0.0f, 0.0f, 1.0f },
        vec3 { 0.0f, 0.0f, -1.0f },
        vec3 { 0.0f, -1.0f, 0.0f },
        vec3 { 0.0f, -1.0f, 0.0f }
    };

    mc::TriangleConsumer fullscreenQuad;
    {
        using mc::util::Vertex;
        fullscreenQuad.start();
        fullscreenQuad.addTriangle(mc::Triangle {
            Vertex { vec3(-1, -1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) },
            Vertex { vec3(+1, -1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) },
            Vertex { vec3(+1, +1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) } });
        fullscreenQuad.addTriangle(mc::Triangle {
            Vertex { vec3(-1, -1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) },
            Vertex { vec3(+1, +1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) },
            Vertex { vec3(-1, +1, 1), vec4(1, 1, 1, 1), vec3(0, 0, -1) } });
        fullscreenQuad.finish();
    }

    CheckGlError("[BlurCubemap] Created fullscreen quad");

    BlurMaterial material(srcCubemap, CreateGaussianKernel(9), blurHalfArcWidth);
    CheckGlError("[BlurCubemap] Created blur material");

    const mat4 projection = perspective<float>(radians(90.0F), 1.0F, 1.0F, 100.0F);
    const mat4 model = mat4 { 1 };

    glViewport(0, 0, size, size);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    std::cout << "[BlurCubemap] - Starting..." << std::endl;
    for (GLuint face = 0; face < 6; ++face) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, destCubemapTexId, 0);
        CheckGlError("[BlurCubemap] Bound face" + std::to_string(face));

        glClear(GL_COLOR_BUFFER_BIT);

        mat4 view = lookAt(vec3 { 0 }, lookAts[face], lookUps[face]);

        // now render using this state
        material.bind(projection, view, model);
        CheckGlError("[BlurCubemap] Bound material while drawing face " + std::to_string(face));
        fullscreenQuad.draw();
        CheckGlError("[BlurCubemap] Drew fullscreen quad while drawing face " + std::to_string(face));
    }
    std::cout << "[BlurCubemap] - Finished!" << std::endl;

    // clean up
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glEnable(GL_DEPTH_TEST);

    CheckGlError("[BlurCubemap] Finished cleanup");

    return std::make_unique<mc::util::TextureHandle>(destCubemapTexId, GL_TEXTURE_CUBE_MAP);
}
