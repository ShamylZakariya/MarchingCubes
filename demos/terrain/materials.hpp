#ifndef materials_hpp
#define materials_hpp

#include <epoxy/gl.h>
#include <mc/util/util.hpp>

using namespace glm;

struct SkyMaterialProperties {
public:
    SkyMaterialProperties()
        : _lightDir(normalize(vec3(0.7, 0.3, 0)))
        , _horizonColor(0.6, 0.8, 1)
        , _spaceColor(0, 0.07, 0.4)
        , _sunColor(1, 0.9, 0.7)
        , _sunsetColor(1, 0.5, 0.67)
    {
    }

    void init(GLuint program)
    {
        _program = program;
        _uLightDir = glGetUniformLocation(program, "uLightDir");
        _uHorizonColor = glGetUniformLocation(program, "uHorizonColor");
        _uSpaceColor = glGetUniformLocation(program, "uSpaceColor");
        _uSunColor = glGetUniformLocation(program, "uSunColor");
        _uSunsetColor = glGetUniformLocation(program, "uSunsetColor");
    }

    void bind()
    {
        glUniform3fv(_uLightDir, 1, value_ptr(_lightDir));
        glUniform3fv(_uHorizonColor, 1, value_ptr(_horizonColor));
        glUniform3fv(_uSpaceColor, 1, value_ptr(_spaceColor));
        glUniform3fv(_uSunColor, 1, value_ptr(_sunColor));
        glUniform3fv(_uSunsetColor, 1, value_ptr(_sunsetColor));
    }

    void setLightDir(const vec3& lightDir) { _lightDir = normalize(lightDir); }
    vec3 getLightDir() const { return _lightDir; }

    void setHorizonColor(const vec3& horizonColor) { _horizonColor = horizonColor; }
    vec3 getHorizonColor() const { return _horizonColor; }

    void setSpaceColor(const vec3& spaceColor) { _spaceColor = spaceColor; }
    vec3 getSpaceColor() const { return _spaceColor; }

    void setSunColor(const vec3& sunColor) { _sunColor = sunColor; }
    vec3 getSunColor() const { return _sunColor; }

    void setSunsetColor(const vec3& sunsetColor) { _sunsetColor = sunsetColor; }
    vec3 getSunsetColor() const { return _sunsetColor; }

private:
    GLuint _program = 0;
    GLint _uLightDir = -1;
    GLint _uHorizonColor = -1;
    GLint _uSpaceColor = -1;
    GLint _uSunColor = -1;
    GLint _uSunsetColor = -1;
    vec3 _lightDir;
    vec3 _horizonColor;
    vec3 _spaceColor;
    vec3 _sunColor;
    vec3 _sunsetColor;
};

struct TerrainMaterial {
public:
    TerrainMaterial(
        float roundWorldRadius, vec3 ambientLight,
        mc::util::TextureHandleRef lightProbe,
        mc::util::TextureHandleRef reflectionMap,
        mc::util::TextureHandleRef texture0, float tex0Scale,
        mc::util::TextureHandleRef texture1, float tex1Scale)
        : _lightprobe(lightProbe)
        , _reflectionMap(reflectionMap)
        , _texture0(texture0)
        , _texture1(texture1)
        , _roundWorldRadius(roundWorldRadius)
        , _ambientLight(ambientLight)
        , _texture0Scale(tex0Scale)
        , _texture1Scale(tex1Scale)
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/terrain.glsl");
        _uVP = glGetUniformLocation(_program, "uVP");
        _uModelTranslation = glGetUniformLocation(_program, "uModelTranslation");
        _uCameraPos = glGetUniformLocation(_program, "uCameraPosition");
        _uRoundWorldRadius = glGetUniformLocation(_program, "uRoundWorldRadius");
        _uLightprobeSampler = glGetUniformLocation(_program, "uLightprobeSampler");
        _uAmbientLight = glGetUniformLocation(_program, "uAmbientLight");
        _uReflectionMapSampler = glGetUniformLocation(_program, "uReflectionMapSampler");
        _uReflectionMapMipLevels = glGetUniformLocation(_program, "uReflectionMapMipLevels");
        _uTexture0Sampler = glGetUniformLocation(_program, "uTexture0Sampler");
        _uTexture0Scale = glGetUniformLocation(_program, "uTexture0Scale");
        _uTexture1Sampler = glGetUniformLocation(_program, "uTexture1Sampler");
        _uTexture1Scale = glGetUniformLocation(_program, "uTexture1Scale");

        _skyMaterialProperties.init(_program);
    }

    TerrainMaterial(const TerrainMaterial& other) = delete;
    TerrainMaterial(const TerrainMaterial&& other) = delete;

    ~TerrainMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void bind(const vec3& modelTranslation, const mat4& view, const mat4& projection, const vec3& cameraPosition)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _lightprobe->getId());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _reflectionMap->getId());

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _texture0->getId());

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, _texture1->getId());

        glUseProgram(_program);
        _skyMaterialProperties.bind();

        glUniformMatrix4fv(_uVP, 1, GL_FALSE, value_ptr(projection * view));
        glUniform3fv(_uModelTranslation, 1, value_ptr(modelTranslation));
        glUniform3fv(_uCameraPos, 1, value_ptr(cameraPosition));
        glUniform1f(_uRoundWorldRadius, _roundWorldRadius);
        glUniform1i(_uLightprobeSampler, 0);
        glUniform3fv(_uAmbientLight, 1, value_ptr(_ambientLight));
        glUniform1i(_uReflectionMapSampler, 1);
        glUniform1f(_uReflectionMapMipLevels, static_cast<float>(_reflectionMap->getMipLevels()));
        glUniform1i(_uTexture0Sampler, 2);
        glUniform1i(_uTexture1Sampler, 3);
        glUniform1f(_uTexture0Scale, _texture0Scale);
        glUniform1f(_uTexture1Scale, _texture1Scale);
    }

    void setWorldRadius(float radius) { _roundWorldRadius = std::max(radius, 0.0F); }
    float getWorldRadius() const { return _roundWorldRadius; }

    SkyMaterialProperties &getSkyMaterial() { return _skyMaterialProperties; }
    const SkyMaterialProperties &getSkyMaterial() const { return _skyMaterialProperties; }

private:
    GLuint _program = 0;
    GLint _uVP = -1;
    GLint _uModelTranslation = -1;
    GLint _uCameraPos = -1;
    GLint _uLightprobeSampler = -1;
    GLint _uTexture0Sampler = -1;
    GLuint _uTexture0Scale = -1;
    GLint _uTexture1Sampler = -1;
    GLuint _uTexture1Scale = -1;
    GLint _uAmbientLight;
    GLint _uReflectionMapSampler = -1;
    GLint _uReflectionMapMipLevels = -1;
    GLint _uRoundWorldRadius = -1;

    mc::util::TextureHandleRef _lightprobe, _reflectionMap, _texture0, _texture1;
    float _roundWorldRadius = 0;
    vec3 _ambientLight;
    float _texture0Scale = 1;
    float _texture1Scale = 1;

    SkyMaterialProperties _skyMaterialProperties;
};

struct LineMaterial {
public:
    LineMaterial()
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/line.glsl");
        _uMVP = glGetUniformLocation(_program, "uMVP");
    }
    LineMaterial(const LineMaterial& other) = delete;
    LineMaterial(const LineMaterial&& other) = delete;

    ~LineMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }
    void bind(const mat4& mvp)
    {
        glUseProgram(_program);
        glUniformMatrix4fv(_uMVP, 1, GL_FALSE, value_ptr(mvp));
    }

private:
    GLuint _program = 0;
    GLint _uMVP = -1;
};

#endif