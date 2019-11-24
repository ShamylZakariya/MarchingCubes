#version 330 core
#extension GL_OES_standard_derivatives : enable

in VS_OUT
{
    vec3 color;
    vec3 normal;
    vec3 barycentric;
} fs_in;

uniform float uWireframe;
out vec4 fragColor;

/////////////////////////////////////////////

// http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
float edgeFactor(){
    vec3 d = fwidth(fs_in.barycentric);
    vec3 a3 = smoothstep(vec3(0.0), d*1.5, fs_in.barycentric);
    return min(min(a3.x, a3.y), a3.z);
}

void main() {
    vec3 normal = normalize(fs_in.normal);
    float wireframeAlpha = (1.0-edgeFactor())*0.95;
    float alpha = mix(1, wireframeAlpha, uWireframe);
    fragColor = vec4(fs_in.color, alpha);
}
