#version 330

in VS_OUT
{
    vec3 rayDir;
} fs_in;

uniform samplerCube uSkyboxSampler;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 color = texture(uSkyboxSampler, rayDir).rgb;
    fragColor = vec4(color,1);
}
