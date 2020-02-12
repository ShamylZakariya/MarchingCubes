#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;

out VS_OUT
{
    vec4 color;
    vec3 normal;
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
    vs_out.normal = inNormal;
    vs_out.worldNormal = mat3(uModel) * inNormal;
    vs_out.worldPosition = vec3(uModel * vec4(inPosition, 1.0));
}
