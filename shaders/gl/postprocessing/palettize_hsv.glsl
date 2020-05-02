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

// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl

// All components are in the range [0…1], including hue.
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    // vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    // vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
    vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec3 rgbIn = texture(uColorTexSampler, fs_in.texCoord).rgb;
    vec3 hsv = rgb2hsv(rgbIn);
    hsv = vec3( ivec3(hsv * uPaletteSize) / uPaletteSize);
    vec3 rgbOut = hsv2rgb(hsv);
    fragColor = vec4(mix(rgbIn, rgbOut, uAlpha), 1);
}
