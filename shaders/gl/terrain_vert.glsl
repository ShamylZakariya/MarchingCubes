#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inTriangleNormal;
layout(location = 3) in float inShininess;
layout(location = 4) in float inTex0Contribution;
layout(location = 5) in float inTex1Contribution;

out VS_OUT
{
    vec4 color;
    vec3 modelPosition;
    vec3 worldNormal;
    vec3 worldPosition;
    float shininess;
    float tex0Contribution;
    float tex1Contribution;
}
vs_out;

uniform mat4 uMVP;
uniform mat4 uModel;

void main()
{
    gl_Position = uMVP * vec4(inPosition, 1.0);

    vs_out.color = inColor;
    vs_out.modelPosition = inPosition;
    vs_out.worldNormal = mat3(uModel) * inTriangleNormal;
    vs_out.worldPosition = vec3(uModel * vec4(inPosition, 1.0));
    vs_out.shininess = inShininess;
    vs_out.tex0Contribution = inTex0Contribution;
    vs_out.tex1Contribution = inTex1Contribution;
}
