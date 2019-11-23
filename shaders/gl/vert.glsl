#version 330 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
    vec3 color;
} vs_out;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(inPosition, 1.0);
    vs_out.texCoord = inTexCoord;
    vs_out.color = inColor;
}
