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
        _program = CreateProgramFromFiles("shaders/gl/skydome_vert.glsl", "shaders/gl/skydome_frag.glsl");
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

struct VolumeMaterial {
private:
    GLuint _program = 0;
    GLint _uMVP = -1;
    GLint _uModel = -1;
    GLint _uCameraPos = -1;
    GLint _uLightprobeSampler = -1;
    GLint _uAmbientLight;
    GLint _uReflectionMapSampler = -1;
    GLint _uReflectionMapMipLevels = -1;
    GLint _uShininess = -1;
    GLint _uDotCreaseThreshold = -1;

    mc::util::TextureHandleRef _lightprobe;
    vec3 _ambientLight;
    mc::util::TextureHandleRef _reflectionMap;
    float _shininess;
    float _creaseThresholdRadians;

public:
    VolumeMaterial(mc::util::TextureHandleRef lightProbe, vec3 ambientLight, mc::util::TextureHandleRef reflectionMap, float shininess)
        : _lightprobe(lightProbe)
        , _ambientLight(ambientLight)
        , _reflectionMap(reflectionMap)
        , _shininess(clamp(shininess, 0.0F, 1.0F))
        , _creaseThresholdRadians(radians<float>(15))
    {
        using namespace mc::util;
        _program = CreateProgramFromFiles("shaders/gl/volume_vert.glsl", "shaders/gl/volume_frag.glsl");
        _uMVP = glGetUniformLocation(_program, "uMVP");
        _uModel = glGetUniformLocation(_program, "uModel");
        _uCameraPos = glGetUniformLocation(_program, "uCameraPosition");
        _uLightprobeSampler = glGetUniformLocation(_program, "uLightprobeSampler");
        _uAmbientLight = glGetUniformLocation(_program, "uAmbientLight");
        _uReflectionMapSampler = glGetUniformLocation(_program, "uReflectionMapSampler");
        _uReflectionMapMipLevels = glGetUniformLocation(_program, "uReflectionMapMipLevels");
        _uShininess = glGetUniformLocation(_program, "uShininess");
        _uDotCreaseThreshold = glGetUniformLocation(_program, "uDotCreaseThreshold");
    }

    VolumeMaterial(const VolumeMaterial& other) = delete;
    VolumeMaterial(const VolumeMaterial&& other) = delete;

    ~VolumeMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void setShininess(float s)
    {
        _shininess = clamp<float>(s, 0, 1);
    }

    float shininess() const { return _shininess; }

    void setCreaseThreshold(float thresholdRadians)
    {
        _creaseThresholdRadians = std::max<float>(thresholdRadians, 0);
    }

    float creaseThreshold() const { return _creaseThresholdRadians; }

    void bind(const mat4& model, const mat4& view, const mat4& projection, const vec3& cameraPosition)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _lightprobe->id());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _reflectionMap->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uMVP, 1, GL_FALSE, value_ptr(projection * model * view));
        glUniformMatrix4fv(_uModel, 1, GL_FALSE, value_ptr(model));
        glUniform3fv(_uCameraPos, 1, value_ptr(cameraPosition));
        glUniform1i(_uLightprobeSampler, 0);
        glUniform3fv(_uAmbientLight, 1, value_ptr(_ambientLight));
        glUniform1i(_uReflectionMapSampler, 1);
        glUniform1f(_uReflectionMapMipLevels, static_cast<float>(_reflectionMap->mipLevels()));
        glUniform1f(_uShininess, _shininess);
        glUniform1f(_uDotCreaseThreshold, cos(_creaseThresholdRadians));
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
        _program = CreateProgramFromFiles("shaders/gl/line_vert.glsl", "shaders/gl/line_frag.glsl");
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