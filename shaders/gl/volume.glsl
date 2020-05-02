vertex:
#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inTriangleNormal;

out VS_OUT
{
    vec4 color;
    vec3 worldNormal;
    vec3 worldPosition;
}
vs_out;

uniform mat4 uMVP;
uniform mat4 uModel;

void main()
{
    gl_Position = uMVP * vec4(inPosition, 1.0);
    vs_out.color = inColor;

    vs_out.worldNormal = mat3(uModel) * inTriangleNormal;
    vs_out.worldPosition = vec3(uModel * vec4(inPosition, 1.0));
}

fragment:
#version 330

in VS_OUT
{
    vec4 color;
    vec3 worldNormal;
    vec3 worldPosition;
}
fs_in;

uniform vec3 uCameraPosition;
uniform samplerCube uLightprobeSampler;
uniform vec3 uAmbientLight;
uniform samplerCube uReflectionMapSampler;
uniform float uReflectionMapMipLevels;
uniform float uShininess;

out vec4 fragColor;

/////////////////////////////////////////////

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));

    float mipLevel = mix(uReflectionMapMipLevels, 0, uShininess);
    vec3 reflectionColor = textureLod(uReflectionMapSampler, R, mipLevel).rgb;

    vec3 lightProbeColor = texture(uLightprobeSampler, fs_in.worldNormal).rgb;

    vec3 color = (fs_in.color.rgb * uAmbientLight) + mix(lightProbeColor, reflectionColor, uShininess);

    fragColor = vec4(color, fs_in.color.a);
}
