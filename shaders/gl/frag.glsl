#version 330 core

uniform sampler2D uTexSampler;

in VS_OUT
{
    vec2 texCoord;
    vec3 color;
} fs_in;

out vec4 fragColor;

void main() {
    fragColor = vec4(fs_in.color * texture(uTexSampler, fs_in.texCoord).rgb, 1.0);
}
