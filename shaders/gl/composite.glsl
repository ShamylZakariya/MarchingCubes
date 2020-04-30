vertex:
#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
} vs_out;

void main() {
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vs_out.texCoord = inTexCoord;
}

fragment:
#version 330

in VS_OUT
{
    vec2 texCoord;
} fs_in;

out vec4 fragColor;

uniform sampler2D uColorTexSampler;

void main() {
    fragColor = texture(uColorTexSampler, fs_in.texCoord);
}
