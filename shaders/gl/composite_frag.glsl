#version 330

in VS_OUT
{
    vec2 texCoord;
} fs_in;

uniform sampler2D uColorTexSampler;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    fragColor = texture(uColorTexSampler, fs_in.texCoord);
}
