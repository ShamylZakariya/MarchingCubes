#version 330

in VS_OUT
{
    vec3 rayDir;
} fs_in;

uniform samplerCube uSrcCubemapSampler;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 rayColor = ((rayDir * 0.5) + vec3(0.5));
    vec3 color = texture(uSrcCubemapSampler, rayDir).rgb;
    color = color * rayColor;
    fragColor = vec4(color,1);
}
