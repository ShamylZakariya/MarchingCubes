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
uniform vec3 uAmbientLight;

out vec4 fragColor;

/////////////////////////////////////////////

void main()
{
    vec3 I = normalize(fs_in.worldPosition - uCameraPosition);
    vec3 R = reflect(I, normalize(fs_in.worldNormal));

    vec3 reflectionColor = texture(uSkyboxSampler, R, uReflectionBlurLevel).rgb;
    reflectionColor = mix(vec3(0), reflectionColor, uShininess);

    vec3 lightProbeColor = texture(uSkyboxSampler, fs_in.worldNormal, 4).rgb;

    vec3 color = (fs_in.color.rgb * uAmbientLight) + lightProbeColor + reflectionColor;

    fragColor = vec4(color, fs_in.color.a);
}
