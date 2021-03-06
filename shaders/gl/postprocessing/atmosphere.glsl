vertex:
#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
} vs_out;

uniform mat4 uProjectionInverse;
uniform mat4 uViewInverse;

void main()
{
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vs_out.texCoord = inTexCoord;

    vec4 r = vec4(inPosition.xy, 0, 1);
    r = uProjectionInverse * r;
    r.w = 0;
    r = uViewInverse * r;
    vs_out.rayDir = vec3(r);
}

fragment:
#version 330

#include "shaders/gl/sky.glsl"

in VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
} fs_in;

const float kGoldenRatioConjugate = 0.61803398875f; // also just fract(goldenRatio)

out vec4 fragColor;

uniform sampler2D uColorSampler;
uniform sampler2D uDepthSampler;
uniform sampler2D uWhiteNoiseSampler;
uniform sampler2D uBlueNoiseSampler;

uniform mat4 uProjectionInverse;
uniform mat4 uViewInverse;
uniform vec3 uCameraPosition;
uniform vec2 uResolution;
uniform float uNearRenderDistance;
uniform float uFarRenderDistance;
uniform float uNearPlane;
uniform float uFarPlane;

uniform float uWorldRadius;
uniform float uGroundFogMaxHeight;
uniform vec4 uGroundFogColor;
uniform vec3 uGroundFogWorldOffset;
uniform vec3 uAmbientLight;
uniform int uFrameCount;

float noise(in vec3 x)
{
    float z = x.z * 64.0;
    vec2 offz = vec2(0.317, 0.123);
    vec2 uv1 = x.xy + offz * floor(z);
    vec2 uv2 = uv1 + offz;
    return mix(textureLod(uWhiteNoiseSampler, uv1, 0.0).x, textureLod(uWhiteNoiseSampler, uv2, 0.0).x, fract(z)) - 0.5;
}

float noises( in vec3 p){
	float a = 0.0;
	for(float i=1.0;i<4.0;i++){
		a += noise(p)/i;
		p = p*2.0 + vec3(0.0,a*0.001/i,a*0.0001/i);
	}
	return a;
}

vec3 getFragmentWorldPosition()
{
    // https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
    float depth = texture(uDepthSampler, fs_in.texCoord).r;
    float z = depth * 2.0 - 1.0;
    vec4 clipSpacePosition = vec4(fs_in.texCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = uProjectionInverse * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = uViewInverse * viewSpacePosition;
    return worldSpacePosition.xyz;
}

vec4 getFogContribution(float sceneDepth, vec3 fragmentWorldPosition, vec3 rayDir, vec3 skyboxColor)
{
    //https://blog.demofox.org/2020/05/10/ray-marching-fog-with-blue-noise/
    float offset = texture(uBlueNoiseSampler, gl_FragCoord.xy / 1024.0f).r;
    offset = fract(offset + float(uFrameCount % 64) * kGoldenRatioConjugate);

    // March the volume defined by noises(), from back to front, with a nudge
    // based on the blue noise offset to reduce appearance of aliasing
    vec3 worldOrigin = vec3(uCameraPosition.x, -uWorldRadius, uCameraPosition.z);
    float dist = min(uFarRenderDistance, sceneDepth);
    dist *= 1 - 0.125 * offset;

    vec3 marchPosition = uCameraPosition + rayDir * dist;
    int steps = 8;
    float density = 4.0 * dist / uFarRenderDistance;
    vec3 marchDir = uCameraPosition - marchPosition;
    float rayLength = length(marchDir);
    vec3 marchIncrement = marchDir / steps;
    float lengthIncrement = rayLength / steps;

    vec3 color = skyboxColor;
    vec3 fogColor = uAmbientLight * uGroundFogColor.rgb;
    float totalContribution = 0;
    for (int i = 0; i < steps; i++) {
        // sample fog at march position
        if (rayLength < sceneDepth) {
            vec3 samplePosition = marchPosition + uGroundFogWorldOffset;
            float c = noises(samplePosition * 0.001);
            c = (c + 1) * 0.5; // remap from [-1,1] to [0,1]
            c = smoothstep(0.35, 1, c);
            c *= density;

            // fade fog out as it approaches the fog height
            float sampleHeight = marchPosition.y;
            if (uWorldRadius > 0) {
                float distanceFromOrigin = distance(marchPosition, worldOrigin);
                sampleHeight = distanceFromOrigin - uWorldRadius;
            }
            float heightFade = 1 - clamp(sampleHeight / uGroundFogMaxHeight, 0, 1);
            c *= heightFade;

            if (c > 0) {
                totalContribution += c;

                // fade fog color to skybox color when it approaches far render distance
                float skyboxMixFactor = rayLength / uFarRenderDistance;
                vec3 contribution = mix(fogColor + skyboxColor, skyboxColor, skyboxMixFactor);
                color = mix(color, contribution, c);
            }
        }
        marchPosition += marchIncrement;
        rayLength -= lengthIncrement;
    }

    return vec4(color, min(totalContribution * uGroundFogColor.a, 1));
}

void main()
{
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 sceneColor = texture(uColorSampler, fs_in.texCoord).rgb;
    vec3 skyboxColor = sky(rayDir, 0);
    vec3 skyboxFogColor = mix(skyboxColor, sky(-rayDir, 0), 0.5);
    vec3 fragmentWorldPosition = getFragmentWorldPosition();
    float sceneDepth = distance(uCameraPosition, fragmentWorldPosition);

    // prevent "popping" by fading in from skybox color to scene color
    float distanceFogContribution = smoothstep(uNearRenderDistance, uFarRenderDistance, sceneDepth);
    sceneColor = mix(sceneColor, skyboxColor, distanceFogContribution);

    vec4 fog = getFogContribution(sceneDepth, fragmentWorldPosition, rayDir, skyboxFogColor);
    sceneColor = mix(sceneColor, fog.rgb, fog.a);
    fragColor = vec4(sceneColor, 1);
}
