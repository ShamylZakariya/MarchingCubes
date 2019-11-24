#version 330 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

out VS_OUT
{
    vec3 color;
    vec3 normal;
} vs_out;

uniform mat4 uMVP;
uniform mat4 uModel;

void main() {
    gl_Position = uMVP * vec4(inPosition, 1.0);
    vs_out.color = inColor;
    vs_out.normal = inNormal;
}
