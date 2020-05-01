#include "post_processing_filters.hpp"

namespace post_processing {

GrayscaleFilter::GrayscaleFilter(const std::string& name)
    : Filter(name)
{
    _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/grayscale.glsl");
    _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
    _uAlpha = glGetUniformLocation(_program, "uAlpha");
}

void GrayscaleFilter::_render(GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<detail::VertexP2T2>& clipspaceQuad)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);

    glUseProgram(_program);
    glUniform1i(_uColorTexSampler, 0);
    glUniform1f(_uAlpha, getAlpha());

    clipspaceQuad.draw();
    glUseProgram(0);
}

}