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

namespace detail {
    void VertexP2T2::bindVertexAttributes()
    {
        glVertexAttribPointer(
            static_cast<GLuint>(AttributeLayout::Pos),
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexP2T2),
            (const GLvoid*)offsetof(VertexP2T2, pos));
        glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Pos));

        glVertexAttribPointer(
            static_cast<GLuint>(AttributeLayout::TexCoord),
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexP2T2),
            (const GLvoid*)offsetof(VertexP2T2, texCoord));
        glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::TexCoord));
    }
}

void Fbo::create(const ivec2& size, bool includeDepth)
{
    if (size == _size)
        return;
    mc::util::CheckGlError("Fbo::create");

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
    mc::util::CheckGlError("Fbo::create - created _colorTex");

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);
    mc::util::CheckGlError("Fbo::create - assigned _colorTex");

    if (includeDepth) {
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        mc::util::CheckGlError("Fbo::create - created _depthTex");

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
        mc::util::CheckGlError("Fbo::create - assigned _depthTex");
    }

    // Check if current configuration of framebuffer is correct
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("[FilterStack::Fbo::create] - Framebuffer not complete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mc::util::CheckGlError("Fbo::create - create - DONE");
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
    _size = ivec2 { 0, 0 };
}

void Filter::_execute(FboRelay& relay, GLuint depthTex)
{
    auto srcFbo = relay.getSrc();
    auto dstFbo = relay.getDst();

    // bind the destination Fbo
    glBindFramebuffer(GL_FRAMEBUFFER, dstFbo->getFramebuffer());
    glViewport(0, 0, _size.x, _size.y);

    if (_clearsColorBuffer) {
        glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    _render(srcFbo->getColorTex(), depthTex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // prepare for next pass
    relay.next();
}

FilterStack::FilterStack()
{
}

unowned_ptr<Fbo> FilterStack::capture(glm::ivec2 captureSize, std::function<void()> renderFunc)
{
    mc::util::CheckGlError("FilterStack::capture");
    if (captureSize != _captureFbo.getSize()) {
        // recreate FBOs, and resize filters
        // Note: only the capture buffer has a depth attachment, it's not needed
        // for filter stack processing, so the other buffer is color only.
        _captureFbo.create(captureSize, true);
        _buffer.create(captureSize, false);

        for (const auto& f : _filters) {
            f->_size = captureSize;
            f->_resize(captureSize);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _captureFbo.getFramebuffer());
    glViewport(0, 0, _captureFbo.getSize().x, _captureFbo.getSize().y);
    mc::util::CheckGlError("FilterStack::capture - bound framebuffer and set viewport");

    renderFunc();
    mc::util::CheckGlError("FilterStack::capture - renderFunc");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return &_captureFbo;
}

unowned_ptr<Fbo> FilterStack::execute(unowned_ptr<Fbo> source)
{

    // TODO: Make a custom VertexP2TC2 format

    // TODO: Optimization opportinity here; when the use-case is capture->execute->composite to screen
    // the final filter can treat the screen as its destination

    bool didExecute = false;
    FboRelay relay(source, &_buffer);
    glDepthMask(GL_FALSE);
    if (!_filters.empty()) {
        for (const auto& f : _filters) {
            if (f->getAlpha() >= kAlphaEpsilon) {
                f->_execute(relay, source->getDepthTex());
                didExecute = true;
            }
        }
    }

    glDepthMask(GL_TRUE);
    return didExecute ? relay.getDst() : source;
}

void FilterStack::draw(unowned_ptr<Fbo> source)
{
    // lazily build composite material and clipspace quad
    if (!_compositeMaterial) {
        _compositeMaterial = std::make_unique<CompositeMaterial>();

        // build a quad that fills the viewport in clip space
        using V = decltype(_clipspaceQuad)::vertex_type;
        _clipspaceQuad.start();
        _clipspaceQuad.addTriangle(mc::Triangle {
            V { vec2(-1, -1), vec2(0, 0) },
            V { vec2(+1, -1), vec2(1, 0) },
            V { vec2(+1, +1), vec2(1, 1) } });
        _clipspaceQuad.addTriangle(mc::Triangle {
            V { vec2(-1, -1), vec2(0, 0) },
            V { vec2(+1, +1), vec2(1, 1) },
            V { vec2(-1, +1), vec2(0, 1) } });
        _clipspaceQuad.finish();
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    _compositeMaterial->bind(source->getColorTex());
    _clipspaceQuad.draw();
    glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////

FilterStack::CompositeMaterial::CompositeMaterial()
{
    using namespace mc::util;
    _program = CreateProgramFromFile("shaders/gl/composite.glsl");
    _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
}

FilterStack::CompositeMaterial::~CompositeMaterial()
{
    glDeleteProgram(_program);
}

void FilterStack::CompositeMaterial::bind(GLuint colorTex)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);

    glUseProgram(_program);
    glUniform1i(_uColorTexSampler, 0);
}

} // namespace post_processing