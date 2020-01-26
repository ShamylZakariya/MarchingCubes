#version 330

in VS_OUT
{
    vec4 color;
    vec3 normal;
    vec3 worldNormal;
} fs_in;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    fragColor = fs_in.color;
}
