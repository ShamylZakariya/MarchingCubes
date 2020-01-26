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
    fragColor = fs_in.color * vec4(((fs_in.normal + 1) * 0.5), 1);
}
