vertex:
#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inTriangleNormal;
layout(location = 3) in float inShininess;
layout(location = 4) in float inTex0Contribution;
layout(location = 5) in float inTex1Contribution;

out VS_OUT
{
    vec4 color;
    vec3 modelPosition;
    vec3 worldNormal;
    vec3 worldPosition;
    float shininess;
    float tex0Contribution;
    float tex1Contribution;
}
vs_out;

uniform mat4 uVP;
uniform vec3 uModelTranslation;
uniform vec3 uCameraPosition;
uniform float uRoundWorldRadius;

void main()
{
    // apply a "pherical" transform to the world. Note: This is a cheap hack,
    // since we're not spherizing, rather, projecting the world to a half-dome
    // facing up and centered beneath the camera.
    vec3 worldPosition = inPosition + uModelTranslation;
    vec3 worldOrigin = vec3(uCameraPosition.x, -uRoundWorldRadius, uCameraPosition.z);
    vec3 worldDir = normalize(worldPosition - worldOrigin);
    float radius = uRoundWorldRadius + worldPosition.y;
    vec3 roundWorldPosition = worldOrigin + worldDir * radius;
    worldPosition = roundWorldPosition;

    gl_Position = uVP * vec4(worldPosition, 1.0);

    vs_out.color = inColor;
    vs_out.modelPosition = inPosition;
    vs_out.shininess = inShininess;
    vs_out.tex0Contribution = inTex0Contribution;
    vs_out.tex1Contribution = inTex1Contribution;
    vs_out.worldNormal = inTriangleNormal;
    vs_out.worldPosition = worldPosition;

    // #if ROUND

}

fragment:
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

#define SHOW_NORMALS 0

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
    vec3 R = reflect(I, fs_in.worldNormal);

    float mipLevel = mix(uReflectionMapMipLevels, 0, fs_in.shininess);
    vec3 reflectionColor = textureLod(uReflectionMapSampler, R, mipLevel).rgb;
    vec3 lightProbeLight = texture(uLightprobeSampler, fs_in.worldNormal).rgb;

#if SHOW_NORMALS
    vec3 color = fs_in.worldNormal * 0.5 + 0.5;
#else
    vec3 diffuse0 = mix(fs_in.color, fs_in.color * TriPlanarTexture(uTexture0Sampler, uTexture0Scale), fs_in.tex0Contribution).rgb;
    vec3 diffuse1 = mix(fs_in.color, fs_in.color * TriPlanarTexture(uTexture1Sampler, uTexture1Scale), fs_in.tex1Contribution).rgb;
    vec3 color = 0.5 * (diffuse0 + diffuse1);
#endif
    color *= uAmbientLight + lightProbeLight;
    color = mix(color, reflectionColor, fs_in.shininess);

    fragColor = vec4(color, 1);
}
