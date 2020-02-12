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
uniform samplerCube uSkyboxSampler;
uniform float uReflectionBlurLevel;
uniform float uShininess;

out vec4 fragColor;

/////////////////////////////////////////////

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));
    vec3 reflectionColor = texture(uSkyboxSampler, R, uReflectionBlurLevel).rgb;
    reflectionColor = mix(vec3(1), reflectionColor, uShininess);

    fragColor = fs_in.color * vec4(reflectionColor, 1);
}
