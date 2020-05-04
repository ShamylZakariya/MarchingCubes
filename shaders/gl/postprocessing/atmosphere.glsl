vertex:
#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
} vs_out;

uniform mat4 uProjectionInverse;
uniform mat4 uViewInverse;

void main() {
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vs_out.texCoord = inTexCoord;

    vec4 r = vec4(inPosition.xy, 0, 1);
    r = uProjectionInverse * r;
    r.w = 0;
    r = uViewInverse * r;
    vs_out.rayDir = vec3(r);
}

fragment:
#version 330

in VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
} fs_in;

out vec4 fragColor;

uniform sampler2D uColorSampler;
uniform sampler2D uDepthSampler;
uniform samplerCube uSkyboxSampler;
uniform vec4 uAtmosphericTint;
uniform float uNearRenderDistance;
uniform float uFarRenderDistance;
uniform float uNearPlane;
uniform float uFarPlane;

float getSceneDepth() {
    // https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer
    float z_b = texture(uDepthSampler, fs_in.texCoord).r;
    float z_n = 2.0 * z_b - 1.0;
    float z_e = 2.0 * uNearPlane * uFarPlane / (uFarPlane + uNearPlane - z_n * (uFarPlane - uNearPlane));
    return z_e;
}

void main() {
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 sceneColor = texture(uColorSampler, fs_in.texCoord).rgb;
    vec3 skyboxColor = texture(uSkyboxSampler, rayDir).rgb;
    float sceneDepth = getSceneDepth();

    float s = smoothstep(uNearRenderDistance, uFarRenderDistance, sceneDepth);
    float s2 = smoothstep(0, uFarRenderDistance, sceneDepth);
    vec3 distanceTintedSceneColor = mix(sceneColor, uAtmosphericTint.rgb, s2 * uAtmosphericTint.a);
    fragColor = vec4(mix(distanceTintedSceneColor, skyboxColor, s), 1);
}
