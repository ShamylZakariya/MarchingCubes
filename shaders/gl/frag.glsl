#version 330 core

in VS_OUT
{
    vec3 color;
    vec3 normal;
} fs_in;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(fs_in.normal);
    fragColor = vec4(fs_in.color, 1.0);
}
