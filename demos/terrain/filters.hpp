#ifndef filters_hpp
#define filters_hpp

#include <glm/ext.hpp>

#include "../common/post_processing_stack.hpp"
#include "materials.hpp"

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

// https://github.com/felixturner/bad-tv-shader
class BadTvFilter : public post_processing::Filter {
public:
    BadTvFilter(const std::string& name)
        : Filter(name)
    {
        _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/bad_tv.glsl");
        _uColorTexSampler = glGetUniformLocation(_program, "uColorTexSampler");
        _uTime = glGetUniformLocation(_program, "uTime");

        // distortion
        _uDistortion = glGetUniformLocation(_program, "uDistortion");
        _uDistortion2 = glGetUniformLocation(_program, "uDistortion2");
        _uSpeed = glGetUniformLocation(_program, "uSpeed");
        _uRollSpeed = glGetUniformLocation(_program, "uRollSpeed");

        // static
        _uStaticMix = glGetUniformLocation(_program, "uStaticMix");
        _uStaticSize = glGetUniformLocation(_program, "uStaticSize");

        // rgb shift
        _uRgbShiftMix = glGetUniformLocation(_program, "uRgbShiftMix");
        _uRgbShiftAngle = glGetUniformLocation(_program, "uRgbShiftAngle");

        // crt scanline effect
        _uCrtMix = glGetUniformLocation(_program, "uCrtMix");
        _uCrtScanlineMix = glGetUniformLocation(_program, "uCrtScanlineMix");
        _uCrtScanlineCount = glGetUniformLocation(_program, "uCrtScanlineCount");
        _uCrtVignetteMix = glGetUniformLocation(_program, "uCrtVignetteMix");
    }

protected:
    void _update(double deltaT) override
    {
        _time += deltaT;
    }

    void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<post_processing::detail::VertexP2T2>& clipspaceQuad) override
    {
        const float alpha = getAlpha();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);

        glUseProgram(_program);
        glUniform1i(_uColorTexSampler, 0);
        glUniform1f(_uTime, _time);

        // distortion
        glUniform1f(_uDistortion, alpha * _distortion);
        glUniform1f(_uDistortion2, alpha * _distortion2);
        glUniform1f(_uSpeed, alpha * _speed);
        glUniform1f(_uRollSpeed, alpha * _rollSpeed);

        // static
        glUniform1f(_uStaticMix, alpha * _staticMix);
        glUniform1f(_uStaticSize, _staticSize);

        // rgb shift
        glUniform1f(_uRgbShiftMix, alpha * _rgbShiftMix);
        glUniform1f(_uRgbShiftAngle, _rgbShiftAngle);

        // crt scanlines
        glUniform1f(_uCrtMix, alpha * _crtMix);
        glUniform1f(_uCrtScanlineMix, alpha * _crtScanlineMix);
        glUniform1f(_uCrtScanlineCount, _crtScanlineCount);
        glUniform1f(_uCrtVignetteMix, alpha * _crtVignetteMix);

        clipspaceQuad.draw();
        glUseProgram(0);
    }

private:
    GLuint _program = 0;
    GLuint _uColorTexSampler = 0;
    GLint _uTime = -1;
    // distortion
    GLint _uDistortion = -1;
    GLint _uDistortion2 = -1;
    GLint _uSpeed = -1;
    GLint _uRollSpeed = -1;

    // static
    GLint _uStaticMix = -1;
    GLint _uStaticSize = -1;

    // rgb distortion
    GLint _uRgbShiftMix = -1;
    GLint _uRgbShiftAngle = -1;

    // crt effect
    GLint _uCrtMix = -1;
    GLint _uCrtScanlineMix = -1;
    GLint _uCrtScanlineCount = -1;
    GLint _uCrtVignetteMix = -1;

    float _time = 0;
    float _distortion = 3;
    float _distortion2 = 5;
    float _speed = 0.2;
    float _rollSpeed = 0;
    float _staticMix = 0.125;
    float _staticSize = 4.0;
    float _rgbShiftMix = 0.005;
    float _rgbShiftAngle = 0;
    float _crtMix = 0.5;
    float _crtScanlineMix = 0.5;
    float _crtScanlineCount = 4096;
    float _crtVignetteMix = 0.35;
};

class AtmosphereFilter : public post_processing::Filter {
public:
    AtmosphereFilter(const std::string& name, mc::util::TextureHandleRef whiteNoise, mc::util::TextureHandleRef blueNoise)
        : Filter(name)
        , _whiteNoise(whiteNoise)
        , _blueNoise(blueNoise)
    {
        _program = mc::util::CreateProgramFromFile("shaders/gl/postprocessing/atmosphere.glsl");
        _skyMaterialProperties.init(_program);

        _uWhiteNoiseSampler = glGetUniformLocation(_program, "uWhiteNoiseSampler");
        _uBlueNoiseSampler = glGetUniformLocation(_program, "uBlueNoiseSampler");
        _uColorSampler = glGetUniformLocation(_program, "uColorSampler");
        _uDepthSampler = glGetUniformLocation(_program, "uDepthSampler");

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

        _uFrameCount = glGetUniformLocation(_program, "uFrameCount");
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

    SkyMaterialProperties& getSkyMaterial() { return _skyMaterialProperties; }
    const SkyMaterialProperties& getSkyMaterial() const { return _skyMaterialProperties; }

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
        glBindTexture(GL_TEXTURE_2D, _whiteNoise->getId());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, _blueNoise->getId());

        glUseProgram(_program);
        _skyMaterialProperties.bind();

        glUniform1i(_uColorSampler, 0);
        glUniform1i(_uDepthSampler, 1);
        glUniform1i(_uWhiteNoiseSampler, 2);
        glUniform1i(_uBlueNoiseSampler, 3);

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

        glUniform1i(_uFrameCount, _frameCount);

        clipspaceQuad.draw();
        glUseProgram(0);

        _frameCount++;
    }

private:
    GLuint _program = 0;

    // TODO: Transition to Uniform Buffer Object to make this one struct
    GLint _uColorSampler = -1;
    GLint _uDepthSampler = -1;
    GLint _uWhiteNoiseSampler = -1;
    GLint _uBlueNoiseSampler = -1;
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
    GLint _uFrameCount;

    mc::util::TextureHandleRef _whiteNoise;
    mc::util::TextureHandleRef _blueNoise;
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
    int _frameCount = 0;

    SkyMaterialProperties _skyMaterialProperties;
};

#endif