#version 330

in VS_OUT
{
    vec4 color;
    vec3 worldNormal;
    vec3 worldPosition;
    float shininess;
    float tex0Contribution;
    float tex1Contribution;
}
fs_in;

uniform vec3 uCameraPosition;
uniform samplerCube uLightprobeSampler;
uniform vec3 uAmbientLight;
uniform samplerCube uReflectionMapSampler;
uniform float uReflectionMapMipLevels;

uniform sampler2D uTexture0Sampler;
uniform float uTexture0Scale;
uniform sampler2D uTexture1Sampler;
uniform float uTexture1Scale;

out vec4 fragColor;

/////////////////////////////////////////////

vec4 TriPlanarTexture(sampler2D sampler, float scale)
{
    vec2 yUV = fs_in.worldPosition.xz / scale;
    vec2 xUV = fs_in.worldPosition.zy / scale;
    vec2 zUV = fs_in.worldPosition.xy / scale;

    vec4 yDiff = texture(sampler, yUV);
    vec4 xDiff = texture(sampler, xUV);
    vec4 zDiff = texture(sampler, zUV);

    // normalized blend weights
    vec3 blendWeights = abs(fs_in.worldNormal);
    blendWeights = blendWeights / (blendWeights.x + blendWeights.y + blendWeights.z);

    return xDiff * blendWeights.x + yDiff * blendWeights.y + zDiff * blendWeights.z;
}

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));

    float mipLevel = mix(uReflectionMapMipLevels, 0, fs_in.shininess);
    vec3 reflectionColor = textureLod(uReflectionMapSampler, R, mipLevel).rgb;

    vec3 lightProbeColor = texture(uLightprobeSampler, fs_in.worldNormal).rgb;

    vec4 diffuse0 = fs_in.color * TriPlanarTexture(uTexture0Sampler, uTexture0Scale);
    vec4 diffuse1 = fs_in.color * TriPlanarTexture(uTexture1Sampler, uTexture1Scale);
    vec3 diffuse = (diffuse0.rgb * fs_in.tex0Contribution + diffuse1.rgb * fs_in.tex1Contribution) / (fs_in.tex0Contribution + fs_in.tex1Contribution);
    vec3 color = diffuse * (uAmbientLight + mix(lightProbeColor, reflectionColor, fs_in.shininess));

    fragColor = vec4(color, fs_in.color.a);
}
