#version 330
#extension GL_OES_standard_derivatives : enable

in VS_OUT
{
    vec3 color;
    vec3 normal;
    vec3 worldNormal;
} fs_in;

uniform float uWireframe;
out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    vec3 color = fs_in.color * ((fs_in.normal + 1) * 0.5);
    fragColor = vec4(color, 1);
}
