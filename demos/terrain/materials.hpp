#ifndef materials_hpp
#define materials_hpp

#include <epoxy/gl.h>
#include <mc/util/util.hpp>

using namespace glm;

struct SkydomeMaterial {
private:
    GLuint _program = 0;
    GLint _uProjectionInverse = -1;
    GLint _uModelViewInverse = -1;
    GLint _uSkyboxSampler = -1;
    mc::util::TextureHandleRef _skyboxTex;

public:
    SkydomeMaterial(mc::util::TextureHandleRef skybox)
        : _skyboxTex(skybox)
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/skydome.glsl");
        _uProjectionInverse = glGetUniformLocation(_program, "uProjectionInverse");
        _uModelViewInverse = glGetUniformLocation(_program, "uModelViewInverse");
        _uSkyboxSampler = glGetUniformLocation(_program, "uSkyboxSampler");
    }

    SkydomeMaterial(const SkydomeMaterial& other) = delete;
    SkydomeMaterial(const SkydomeMaterial&& other) = delete;

    ~SkydomeMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void bind(const mat4& projection, const mat4& modelview)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skyboxTex->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uProjectionInverse, 1, GL_FALSE, value_ptr(inverse(projection)));
        glUniformMatrix4fv(_uModelViewInverse, 1, GL_FALSE, value_ptr(inverse(modelview)));
        glUniform1i(_uSkyboxSampler, 0);
    }
};

struct TerrainMaterial {
private:
    GLuint _program = 0;
    GLint _uMVP = -1;
    GLint _uModel = -1;
    GLint _uCameraPos = -1;
    GLint _uLightprobeSampler = -1;
    GLint _uTexture0Sampler = -1;
    GLuint _uTexture0Scale = -1;
    GLint _uTexture1Sampler = -1;
    GLuint _uTexture1Scale = -1;
    GLint _uAmbientLight;
    GLint _uReflectionMapSampler = -1;
    GLint _uReflectionMapMipLevels = -1;

    mc::util::TextureHandleRef _lightprobe, _reflectionMap, _texture0, _texture1;
    vec3 _ambientLight;
    float _texture0Scale = 1;
    float _texture1Scale = 1;

public:
    TerrainMaterial(
        vec3 ambientLight,
        mc::util::TextureHandleRef lightProbe,
        mc::util::TextureHandleRef reflectionMap,
        mc::util::TextureHandleRef texture0, float tex0Scale,
        mc::util::TextureHandleRef texture1, float tex1Scale)
        : _lightprobe(lightProbe)
        , _reflectionMap(reflectionMap)
        , _texture0(texture0)
        , _texture1(texture1)
        , _ambientLight(ambientLight)
        , _texture0Scale(tex0Scale)
        , _texture1Scale(tex1Scale)
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/terrain.glsl");
        _uMVP = glGetUniformLocation(_program, "uMVP");
        _uModel = glGetUniformLocation(_program, "uModel");
        _uCameraPos = glGetUniformLocation(_program, "uCameraPosition");
        _uLightprobeSampler = glGetUniformLocation(_program, "uLightprobeSampler");
        _uAmbientLight = glGetUniformLocation(_program, "uAmbientLight");
        _uReflectionMapSampler = glGetUniformLocation(_program, "uReflectionMapSampler");
        _uReflectionMapMipLevels = glGetUniformLocation(_program, "uReflectionMapMipLevels");
        _uTexture0Sampler = glGetUniformLocation(_program, "uTexture0Sampler");
        _uTexture0Scale = glGetUniformLocation(_program, "uTexture0Scale");
        _uTexture1Sampler = glGetUniformLocation(_program, "uTexture1Sampler");
        _uTexture1Scale = glGetUniformLocation(_program, "uTexture1Scale");
    }

    TerrainMaterial(const TerrainMaterial& other) = delete;
    TerrainMaterial(const TerrainMaterial&& other) = delete;

    ~TerrainMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void bind(const mat4& model, const mat4& view, const mat4& projection, const vec3& cameraPosition)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _lightprobe->id());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _reflectionMap->id());

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _texture0->id());

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, _texture1->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uMVP, 1, GL_FALSE, value_ptr(projection * view * model));
        glUniformMatrix4fv(_uModel, 1, GL_FALSE, value_ptr(model));
        glUniform3fv(_uCameraPos, 1, value_ptr(cameraPosition));
        glUniform1i(_uLightprobeSampler, 0);
        glUniform3fv(_uAmbientLight, 1, value_ptr(_ambientLight));
        glUniform1i(_uReflectionMapSampler, 1);
        glUniform1f(_uReflectionMapMipLevels, static_cast<float>(_reflectionMap->mipLevels()));
        glUniform1i(_uTexture0Sampler, 2);
        glUniform1i(_uTexture1Sampler, 3);
        glUniform1f(_uTexture0Scale, _texture0Scale);
        glUniform1f(_uTexture1Scale, _texture1Scale);
    }
};

struct LineMaterial {
private:
    GLuint _program = 0;
    GLint _uMVP = -1;

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
};

#endif