#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

out VS_OUT
{
    vec3 rayDir;
} vs_out;

uniform mat4 uProjectionInverse;
uniform mat4 uModelViewInverse;

void main() {
    vec4 r = vec4(inPosition.xy, 0, 1);
    r = uProjectionInverse * r;
    r.w = 0;
    r = uModelViewInverse * r;
    vs_out.rayDir = vec3(r);

    gl_Position = vec4(inPosition, 1.0);
}
