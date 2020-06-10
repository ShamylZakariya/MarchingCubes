#ifndef filters_hpp
#define filters_hpp

#include <glm/ext.hpp>

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

    void setPixelSize(int pixelSize)
    {
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

class AtmosphereFilter : public post_processing::Filter {
public:
    AtmosphereFilter(const std::string& name, mc::util::TextureHandleRef skybox, mc::util::TextureHandleRef noise)
        : Filter(name)
        , _skybox(skybox)
        , _noise(noise)
    {
        _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/atmosphere.glsl");

        _uNoiseSampler = glGetUniformLocation(_program, "uNoiseSampler");
        _uColorSampler = glGetUniformLocation(_program, "uColorSampler");
        _uDepthSampler = glGetUniformLocation(_program, "uDepthSampler");
        _uSkyboxSampler = glGetUniformLocation(_program, "uSkyboxSampler");

        _uProjectionInverse = glGetUniformLocation(_program, "uProjectionInverse");
        _uViewInverse = glGetUniformLocation(_program, "uViewInverse");
        _uCameraPosition = glGetUniformLocation(_program, "uCameraPosition");
        _uNearRenderDistance = glGetUniformLocation(_program, "uNearRenderDistance");
        _uFarRenderDistance = glGetUniformLocation(_program, "uFarRenderDistance");
        _uNearPlane = glGetUniformLocation(_program, "uNearPlane");
        _uFarPlane = glGetUniformLocation(_program, "uFarPlane");

        _uWorldRadius = glGetUniformLocation(_program, "uWorldRadius");
        _uGroundFogMaxHeight = glGetUniformLocation(_program, "uGroundFogMaxHeight");
        _uGroundFogColor = glGetUniformLocation(_program, "uGroundFogColor");
        _uGroundFogWorldOffset = glGetUniformLocation(_program, "uGroundFogWorldOffset");
    }

    void setCameraState(const glm::vec3 position, const glm::mat4& projection, const glm::mat4& view, float nearPlane, float farPlane)
    {
        _cameraPosition = position;
        _projection = projection;
        _view = view;
        _nearPlane = nearPlane;
        _farPlane = farPlane;
    }

    void setRenderDistance(float nearRenderDistance, float farRenderDistance)
    {
        _nearRenderDistance = nearRenderDistance;
        _farRenderDistance = farRenderDistance;
    }

    void setFog(float maxHeight, glm::vec4 groundFogColor)
    {
        _groundFogMaxHeight = maxHeight;
        _groundFogColor = groundFogColor;
    }

    void setFogWindSpeed(glm::vec3 windSpeed)
    {
        _fogWindSpeed = windSpeed;
    }

    glm::vec3 getFogWindSpeed() const { return _fogWindSpeed; }

    void setGroundFogWorldOffset(glm::vec3 offset)
    {
        _groundFogWorldOffset = offset;
    }

    glm::vec3 getGroundFogWorldOffset() const { return _groundFogWorldOffset; }

    void setWorldRadius(float r)
    {
        _worldRadius = r;
    }

    float getWorldRadius() const { return _worldRadius; }

protected:
    void _update(double deltaT) override
    {
        _groundFogWorldOffset += _fogWindSpeed * deltaT;
    }

    void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<post_processing::detail::VertexP2T2>& clipspaceQuad) override
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skybox->id());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, _noise->id());

        glUseProgram(_program);

        glUniform1i(_uColorSampler, 0);
        glUniform1i(_uDepthSampler, 1);
        glUniform1i(_uSkyboxSampler, 2);
        glUniform1i(_uNoiseSampler, 3);

        glUniformMatrix4fv(_uProjectionInverse, 1, GL_FALSE, glm::value_ptr(glm::inverse(_projection)));
        glUniformMatrix4fv(_uViewInverse, 1, GL_FALSE, glm::value_ptr(glm::inverse(_view)));
        glUniform3fv(_uCameraPosition, 1, glm::value_ptr(_cameraPosition));
        glUniform1f(_uNearRenderDistance, _nearRenderDistance);
        glUniform1f(_uFarRenderDistance, _farRenderDistance);
        glUniform1f(_uNearPlane, _nearPlane);
        glUniform1f(_uFarPlane, _farPlane);

        glUniform1f(_uWorldRadius, _worldRadius);
        glUniform1f(_uGroundFogMaxHeight, _groundFogMaxHeight);
        glUniform4fv(_uGroundFogColor, 1, glm::value_ptr(_groundFogColor));
        glUniform3fv(_uGroundFogWorldOffset, 1, glm::value_ptr(_groundFogWorldOffset));
        CHECK_GL_ERROR("_render 4");

        clipspaceQuad.draw();
        glUseProgram(0);
    }

private:
    GLuint _program = 0;

    // TODO: Transition to Uniform Buffer Object to make this one struct
    GLint _uColorSampler = -1;
    GLint _uDepthSampler = -1;
    GLint _uSkyboxSampler = -1;
    GLint _uNoiseSampler = -1;
    GLint _uProjectionInverse = -1;
    GLint _uViewInverse = -1;
    GLint _uCameraPosition = -1;
    GLint _uNearRenderDistance = -1;
    GLint _uFarRenderDistance = -1;
    GLint _uNearPlane = -1;
    GLint _uFarPlane = -1;
    GLint _uGroundFogMaxHeight = -1;
    GLint _uWorldRadius = -1;
    GLint _uGroundFogColor = -1;
    GLint _uGroundFogWorldOffset = -1;

    mc::util::TextureHandleRef _skybox;
    mc::util::TextureHandleRef _noise;
    glm::mat4 _projection;
    glm::mat4 _view;
    glm::vec3 _cameraPosition;
    float _nearPlane = 0;
    float _farPlane = 0;
    float _nearRenderDistance = 0;
    float _farRenderDistance = 0;
    float _groundFogMaxHeight = 0;
    float _worldRadius = 0;
    glm::vec4 _groundFogColor { 1, 1, 1, 0.5 };
    glm::vec3 _groundFogWorldOffset { 0 };
    glm::vec3 _fogWindSpeed { 0 };
};

#endif