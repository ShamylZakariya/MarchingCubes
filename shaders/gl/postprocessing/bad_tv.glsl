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
// https://github.com/felixturner/bad-tv-shader

in VS_OUT
{
    vec2 texCoord;
} fs_in;

out vec4 fragColor;

uniform float uAlpha;
uniform sampler2D uColorTexSampler;
uniform float uTime;

// distortion
uniform float uDistortion;
uniform float uDistortion2;
uniform float uSpeed;
uniform float uRollSpeed;

// static
uniform float uStaticMix;
uniform float uStaticSize;

// rgb shift
uniform float uRgbShiftMix;
uniform float uRgbShiftAngle;

// crt effect
uniform float uCrtMix;
uniform float uCrtScanlineMix;
uniform float uCrtScanlineCount;
uniform float uCrtVignettMix;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec3 mod289(vec3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(vec2 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x)
{
    return mod289(((x * 34.0) + 1.0) * x);
}

float snoise(vec2 v)
{
    const vec4 C = vec4(0.211324865405187, // (3.0-sqrt(3.0))/6.0
        0.366025403784439, // 0.5*(sqrt(3.0)-1.0)
        -0.577350269189626, // -1.0 + 2.0 * C.x
        0.024390243902439); // 1.0 / 41.0
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);

    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;

    i = mod289(i); // Avoid truncation effects in permutation
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0))
        + i.x + vec3(0.0, i1.x, 1.0));

    vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

    vec3 g;
    g.x = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

vec4 rgbshift(sampler2D colorTex, vec2 coord) {
    vec2 offset = uRgbShiftMix * vec2( cos(uRgbShiftAngle), sin(uRgbShiftAngle));
    vec4 cr = texture2D(colorTex, coord + offset);
    vec4 cga = texture2D(colorTex, coord);
    vec4 cb = texture2D(colorTex, coord - offset);
    return vec4(cr.r, cga.g, cb.b, cga.a);
}

vec4 crt(vec4 color) {
    // make some noise
    float x = fs_in.texCoord.x * fs_in.texCoord.y * uTime *  1000.0;
    x = mod( x, 13.0 ) * mod( x, 123.0 );
    float dx = mod( x, 0.01 );

    // add noise
    vec3 outColor = color.rgb + color.rgb * clamp( 0.1 + dx * 100.0, 0.0, 1.0 );

    // get us a sine and cosine
    vec2 sc = vec2( sin( fs_in.texCoord.y * uCrtScanlineCount ), cos( fs_in.texCoord.y * uCrtScanlineCount ) );

    // add scanlines
    outColor += color.rgb * vec3( sc.x, sc.y, sc.x ) * uCrtScanlineMix;

    outColor = mix(color.rgb, outColor, uCrtMix);
    return vec4(outColor, 1);
}

vec4 vignette(vec4 color, float amt) {
    float r = distance(fs_in.texCoord, vec2(0.5, 0.5));
    float d = r / 0.5;
    return mix(color, vec4(0,0,0,1), amt * d);
}

void main()
{
    vec2 p = fs_in.texCoord;
    float ty = uTime * uSpeed;
    float yt = p.y - ty;
    //smooth distortion
    float offset = snoise(vec2(yt * 3.0, 0.0)) * 0.2;
    // boost distortion
    offset = offset * uDistortion * offset * uDistortion * offset;
    //add fine grain distortion
    offset += snoise(vec2(yt * 50.0, 0.0)) * uDistortion2 * 0.001;
    //combine distortion on X with roll on Y
    vec4 color = rgbshift(uColorTexSampler, vec2(p.x + offset, fract(p.y - uTime * uRollSpeed)));

    // apply static
    float xs = floor(gl_FragCoord.x / uStaticSize);
    float ys = floor(gl_FragCoord.y / uStaticSize);
    vec4 snow = vec4(rand(vec2(xs * uTime, ys * uTime)) * uStaticMix);
    color += snow;

    // crt effect
    color = vignette(crt(color), uCrtVignettMix);

    fragColor = color;
}