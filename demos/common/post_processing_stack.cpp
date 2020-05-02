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

//
// Filter
//

void Filter::_execute(GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<detail::VertexP2T2>& clipspaceQuad)
{
    if (_clearsColorBuffer) {
        glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    _render(colorTex, depthTex, clipspaceQuad);
}

//
// FilterStack
//

FilterStack::FilterStack()
{
    glGenFramebuffers(1, &_fbo);

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

FilterStack::~FilterStack()
{
    destroyAttachments();
    glDeleteFramebuffers(1, &_fbo);
}

void FilterStack::execute(glm::ivec2 captureSize, std::function<void()> renderFunc)
{
    CHECK_GL_ERROR("FilterStack::capture");
    if (captureSize != _size) {
        createAttachments(captureSize);
        for (const auto& f : _filters) {
            f->_size = captureSize;
            f->_resize(captureSize);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTexDst, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);

#ifndef NDEBUG
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("[FilterStack::execute] - Framebuffer not complete");
    }
#endif

    glViewport(0, 0, _size.x, _size.y);
    CHECK_GL_ERROR("FilterStack::execute - bound framebuffer and set viewport");

    renderFunc();
    CHECK_GL_ERROR("FilterStack::execute - renderFunc");

    // remove depth attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    CHECK_GL_ERROR("FilterStack::execute - removed depth attachment");

    GLuint colorTexSrc = _colorTexSrc;
    GLuint colorTexDst = _colorTexDst;
    for (const auto& f : _filters) {
        if (f->getAlpha() >= kAlphaEpsilon) {
            // swap src and dst, and bind fbo to the new dst
            std::swap(colorTexSrc, colorTexDst);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexDst, 0);

#ifndef NDEBUG
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                throw std::runtime_error("[FilterStack::execute] - Framebuffer not complete");
            }
#endif

            f->_execute(colorTexSrc, _depthTex, _clipspaceQuad);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    blit(colorTexDst, _depthTex);
}

void FilterStack::blit(GLuint colorTex, GLuint depthTex)
{
    CHECK_GL_ERROR("FilterStack::blit - Start");

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, _fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glBlitFramebuffer(
        0, 0, _size.x, _size.y,
        0, 0, _size.x, _size.y,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
        GL_NEAREST);

    CHECK_GL_ERROR("FilterStack::blit - DONE");
}

void FilterStack::destroyAttachments()
{
    if (_colorTexSrc)
        glDeleteTextures(1, &_colorTexSrc);
    if (_colorTexDst)
        glDeleteTextures(1, &_colorTexDst);
    if (_depthTex)
        glDeleteTextures(1, &_depthTex);
    _colorTexSrc = _colorTexDst = _depthTex = 0;
}

void FilterStack::createAttachments(glm::ivec2 size)
{
    destroyAttachments();

    glGenTextures(1, &_colorTexSrc);
    glBindTexture(GL_TEXTURE_2D, _colorTexSrc);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &_colorTexDst);
    glBindTexture(GL_TEXTURE_2D, _colorTexDst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &_depthTex);
    glBindTexture(GL_TEXTURE_2D, _depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    CHECK_GL_ERROR("Fbo::create - created _depthTex");

    _size = size;
}

} // namespace post_processing