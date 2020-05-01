#ifndef post_processing_filters_hpp
#define post_processing_filters_hpp

#include "post_processing_stack.hpp"

namespace post_processing {

class GrayscaleFilter : public Filter {
public:
    GrayscaleFilter(const std::string& name);

protected:
    void _render(GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<detail::VertexP2T2>& clipspaceQuad) override;

private:
    GLuint _program = 0;
    GLuint _uColorTexSampler = 0;
    GLuint _uAlpha = 0;
};

}

#endif