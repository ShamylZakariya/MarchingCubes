#version 330

in VS_OUT
{
    vec4 color;
    vec3 modelPosition;
    vec3 worldNormal;
    vec3 worldPosition;
    float shininess;
    float tex0Contribution;
    float tex1Contribution;
}
fs_in;

uniform vec3 uAmbientLight;
uniform vec3 uCameraPosition;
uniform samplerCube uLightprobeSampler;
uniform samplerCube uReflectionMapSampler;
uniform float uReflectionMapMipLevels;

uniform sampler2D uTexture0Sampler;
uniform float uTexture0Scale;
uniform sampler2D uTexture1Sampler;
uniform float uTexture1Scale;

out vec4 fragColor;

/////////////////////////////////////////////

vec4 TriPlanarTexture(sampler2D sampler, float scale)
{
    vec2 yUV = fs_in.modelPosition.xz / scale;
    vec2 xUV = fs_in.modelPosition.zy / scale;
    vec2 zUV = fs_in.modelPosition.xy / scale;

    vec4 yDiff = texture(sampler, yUV);
    vec4 xDiff = texture(sampler, xUV);
    vec4 zDiff = texture(sampler, zUV);

    // normalized blend weights
    vec3 blendWeights = abs(fs_in.worldNormal);
    blendWeights = blendWeights / (blendWeights.x + blendWeights.y + blendWeights.z);

    return xDiff * blendWeights.x + yDiff * blendWeights.y + zDiff * blendWeights.z;
}

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));

    float mipLevel = mix(uReflectionMapMipLevels, 0, fs_in.shininess);
    vec3 reflectionColor = textureLod(uReflectionMapSampler, R, mipLevel).rgb;
    vec3 lightProbeLight = texture(uLightprobeSampler, fs_in.worldNormal).rgb;

    vec3 diffuse0 = mix(fs_in.color, fs_in.color * TriPlanarTexture(uTexture0Sampler, uTexture0Scale), fs_in.tex0Contribution).rgb;
    vec3 diffuse1 = mix(fs_in.color, fs_in.color * TriPlanarTexture(uTexture1Sampler, uTexture1Scale), fs_in.tex1Contribution).rgb;
    vec3 color = 0.5 * (diffuse0 + diffuse1);

    color *= uAmbientLight + lightProbeLight;
    color = mix(color, reflectionColor, fs_in.shininess);

    fragColor = vec4(color, fs_in.color.a);
}
