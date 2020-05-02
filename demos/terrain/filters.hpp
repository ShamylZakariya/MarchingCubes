#ifndef filters_hpp
#define filters_hpp

#include "../common/post_processing_stack.hpp"

class GrayscaleFilter : public post_processing::Filter {
public:
    GrayscaleFilter(const std::string& name)
        : Filter(name)
    {
        _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/grayscale.glsl");
        _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
        _uAlpha = glGetUniformLocation(_program, "uAlpha");
    }

protected:
    void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<post_processing::detail::VertexP2T2>& clipspaceQuad) override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);

        glUseProgram(_program);
        glUniform1i(_uColorTexSampler, 0);
        glUniform1f(_uAlpha, getAlpha());

        clipspaceQuad.draw();
        glUseProgram(0);
    }

private:
    GLuint _program = 0;
    GLuint _uColorTexSampler = 0;
    GLuint _uAlpha = 0;
};

class PalettizeFilter : public post_processing::Filter {
public:
    enum class ColorSpace {
        RGB,
        HSV,
        YUV
    };

    PalettizeFilter(const std::string& name, glm::ivec3 paletteSize, ColorSpace mode)
        : Filter(name)
    {
        setPaletteSize(paletteSize);
        switch (mode) {
        case ColorSpace::RGB:
            _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/palettize_rgb.glsl");
            break;
        case ColorSpace::HSV:
            _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/palettize_hsv.glsl");
            break;
        case ColorSpace::YUV:
            _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/palettize_yuv.glsl");
            break;
        }
        _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
        _uAlpha = glGetUniformLocation(_program, "uAlpha");
        _uPaletteSize = glGetUniformLocation(_program, "uPaletteSize");
    }

    void setPaletteSize(glm::ivec3 paletteSize)
    {
        _paletteSize = glm::clamp(paletteSize, glm::ivec3(0), glm::ivec3(255));
    }

    glm::ivec3 getPaletteSize() const { return _paletteSize; }

protected:
    void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<post_processing::detail::VertexP2T2>& clipspaceQuad) override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);

        glUseProgram(_program);
        glUniform1i(_uColorTexSampler, 0);
        glUniform1f(_uAlpha, getAlpha());
        glUniform3fv(_uPaletteSize, 1, glm::value_ptr(glm::vec3(_paletteSize)));

        clipspaceQuad.draw();
        glUseProgram(0);
    }

private:
    GLuint _program = 0;
    GLuint _uColorTexSampler = 0;
    GLuint _uAlpha = 0;
    GLuint _uPaletteSize = 0;
    glm::ivec3 _paletteSize;
};

class PixelateFilter : public post_processing::Filter {
public:
    PixelateFilter(const std::string& name, int pixelSize)
        : Filter(name)
    {
        setPixelSize(pixelSize);
        _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/pixelate.glsl");
        _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
        _uPixelSize = glGetUniformLocation(_program, "uPixelSize");
        _uOutputSize = glGetUniformLocation(_program, "uOutputSize");
    }

    void setPixelSize(int pixelSize) {
        _pixelSize = std::max(pixelSize, 1);
    }

    int getPixelSize() const { return _pixelSize; }

protected:
    void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<post_processing::detail::VertexP2T2>& clipspaceQuad) override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);

        glUseProgram(_program);
        glUniform1i(_uColorTexSampler, 0);
        glUniform1f(_uPixelSize, round(_pixelSize * getAlpha()));
        glUniform2fv(_uOutputSize, 1, glm::value_ptr(glm::vec2(size)));

        clipspaceQuad.draw();
        glUseProgram(0);
    }

private:
    GLuint _program = 0;
    GLuint _uColorTexSampler = 0;
    GLuint _uPixelSize = 0;
    GLuint _uOutputSize = 0;
    int _pixelSize;
};

#endif