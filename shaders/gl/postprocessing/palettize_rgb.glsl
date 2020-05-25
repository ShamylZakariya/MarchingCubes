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

uniform float uAlpha;
uniform sampler2D uColorTexSampler;
uniform vec3 uPaletteSize;

void main() {
    vec3 rgbIn = texture(uColorTexSampler, fs_in.texCoord).rgb;
    vec3 paletteSize = mix(vec3(255,255,255), uPaletteSize, uAlpha);
    vec3 rgbOut = vec3( ivec3(rgbIn * paletteSize) / paletteSize);
    fragColor = vec4(rgbOut, 1);
}
