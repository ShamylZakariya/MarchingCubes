vertex:
#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
}
vs_out;

void main()
{
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vs_out.texCoord = inTexCoord;
}

fragment:
#version 330

in VS_OUT
{
    vec2 texCoord;
}
fs_in;

out vec4 fragColor;

uniform float uAlpha;
uniform sampler2D uColorTexSampler;
uniform vec3 uPaletteSize;

vec3 rgb2yuv(vec3 rgb)
{
    return vec3(
        (0.257 * rgb.r) + (0.504 * rgb.g) + (0.098 * rgb.b) + 0.0625,
        -(0.148 * rgb.r) - (0.291 * rgb.g) + (0.439 * rgb.b) + 0.5,
        (0.439 * rgb.r) - (0.368 * rgb.g) - (0.071 * rgb.b) + 0.5);
}

vec3 yuv2rgb(vec3 yuv)
{
    return vec3(
        (1.164 * (yuv.x - 0.0625)) + (1.596 * (yuv.z - 0.5)),
        (1.164 * (yuv.x - 0.0625)) - (0.813 * (yuv.z - 0.5)) - (0.391 * (yuv.y - 0.5)),
        (1.164 * (yuv.x - 0.0625)) + (2.018 * (yuv.y - 0.5)));
}

void main()
{
    vec3 rgbIn = texture(uColorTexSampler, fs_in.texCoord).rgb;
    vec3 yuv = rgb2yuv(rgbIn);
    yuv = vec3(ivec3(yuv * uPaletteSize) / uPaletteSize);
    vec3 rgbOut = yuv2rgb(yuv);
    fragColor = vec4(mix(rgbIn, rgbOut, uAlpha), 1);
}
