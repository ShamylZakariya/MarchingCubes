vertex:
#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

out VS_OUT
{
    vec4 color;
} vs_out;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(inPosition, 1.0);
    vs_out.color = inColor;
}

fragment:
#version 330

in VS_OUT
{
    vec4 color;
} fs_in;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    fragColor = fs_in.color;
}
