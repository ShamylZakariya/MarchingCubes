#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <mc/triangle_consumer.hpp>

#include "post_processing_stack.hpp"

using namespace glm;
namespace post_processing {

namespace {
    constexpr float kAlphaEpsilon = 1.0f / 255.0f;
}

void Fbo::create(const ivec2& size, bool includeDepth)
{
    if (size == _size) return;

    // clean up previous configuration
    destroy();

    _size = size;
    glGenTextures(1, &_colorTex);
    glBindTexture(GL_TEXTURE_2D, _colorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);


    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);

    if (includeDepth) {
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
    }

    // Check if current configuration of framebuffer is correct
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("[FilterStack::Fbo::create] - Framebuffer not complete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Fbo::destroy()
{
    if (_colorTex) {
        glDeleteTextures(1, &_colorTex);
        _colorTex = 0;
    }
    if (_depthTex) {
        glDeleteTextures(1, &_depthTex);
        _depthTex = 0;
    }
    if (_fbo) {
        glDeleteFramebuffers(1, &_fbo);
        _fbo = 0;
    }
    _size = ivec2{0,0};
}

void Filter::_execute(FboRelay &relay, GLuint depthTex) {
    auto srcFbo = relay.getSrc();
    auto dstFbo = relay.getDst();

    // bind the destination Fbo
    glBindFramebuffer(GL_FRAMEBUFFER, dstFbo->getFramebuffer());
    glViewport(0,0, _size.x, _size.y);

    if (_clearsColorBuffer) {
        glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    _render(srcFbo->getColorTex(), depthTex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // prepare for next pass
    relay.next();
}

FilterStack::~FilterStack()
{
}

unowned_ptr<Fbo> FilterStack::capture(glm::ivec2 captureSize, std::function<void()> renderFunc) {
    if (captureSize != _captureFbo.getSize()) {
        // recreate FBOs, and resize filters
        _captureFbo.create(captureSize, true);
        _buffer.create(captureSize, false);

        for (const auto& f : _filters) {
            f->_size = captureSize;
            f->_resize(captureSize);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _captureFbo.getFramebuffer());
    glViewport(0,0, _captureFbo.getSize().x, _captureFbo.getSize().y);

    renderFunc();

    return &_captureFbo;
}

unowned_ptr<Fbo> FilterStack::execute(unowned_ptr<Fbo> source) {

    // TODO: I think I should go back to bufferA, bufferB and
    // blit the color tex to bufferA; it's possible though that
    // disabling depth writes obviates this and leaves the depth
    // texture effectively read-only

    FboRelay relay(source, &_buffer);
    if (!_filters.empty()) {
        for (const auto &f : _filters) {
            if (f->getAlpha() >= kAlphaEpsilon) {
                f->_execute(relay, source->getDepthTex());
            }
        }
    }

    return nullptr;
}



} // namespace post_processing