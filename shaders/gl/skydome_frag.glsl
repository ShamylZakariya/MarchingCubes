#version 330

in VS_OUT
{
    vec3 rayDir;
} fs_in;

uniform samplerCube skyboxSampler;

out vec4 fragColor;

/////////////////////////////////////////////

void main() {
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 color = texture(skyboxSampler, rayDir).rgb;

    fragColor = vec4(rayDir * color,1);
}
