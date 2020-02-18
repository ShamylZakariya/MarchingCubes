#version 330

in VS_OUT
{
    vec4 color;
    vec3 normal;
    vec3 worldNormal;
    vec3 worldPosition;
}
fs_in;

uniform vec3 uCameraPosition;
uniform samplerCube uLightprobeSampler;
uniform vec3 uAmbientLight;
uniform samplerCube uReflectionMapSampler;
uniform float uReflectionMapMipLevels;
uniform float uShininess;

out vec4 fragColor;

/////////////////////////////////////////////

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));

    float mipLevel = mix(uReflectionMapMipLevels, 0, uShininess);
    vec3 reflectionColor = textureLod(uReflectionMapSampler, R, mipLevel).rgb;

    vec3 lightProbeColor = texture(uLightprobeSampler, fs_in.worldNormal).rgb;

    vec3 color = (fs_in.color.rgb * uAmbientLight) + mix(lightProbeColor, reflectionColor, uShininess);

    fragColor = vec4(color, fs_in.color.a);
}
