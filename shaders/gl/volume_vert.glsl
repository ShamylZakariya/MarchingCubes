#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inVertexNormal;
layout(location = 3) in vec3 inTriangleNormal;

out VS_OUT
{
    vec4 color;
    vec3 worldNormal;
    vec3 worldPosition;
}
vs_out;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform float uDotCreaseThreshold;

void main()
{
    gl_Position = uMVP * vec4(inPosition, 1.0);
    vs_out.color = inColor;

    // select whether to use the triangle or world normal
    // by comparing how much they differ. If they differ more
    // than the crease threshold, that means we have a sharp crease
    // and want to use the triangle normal to make a hard edge. note,
    // since we're using the dot product of the crease threshold the
    // comparison appears inverted.
    float crease = dot(inTriangleNormal, inVertexNormal);
    float s = step(uDotCreaseThreshold, crease);
    vs_out.worldNormal = mat3(uModel) * mix(inTriangleNormal, inVertexNormal, s);

    vs_out.worldPosition = vec3(uModel * vec4(inPosition, 1.0));
}
