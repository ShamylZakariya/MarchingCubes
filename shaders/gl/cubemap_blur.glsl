vertex:
#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

out VS_OUT
{
    vec3 rayDir;
} vs_out;

uniform mat4 uProjectionInverse;
uniform mat4 uModelViewInverse;

void main() {
    vec4 r = vec4(inPosition.xy, 0, 1);
    r = uProjectionInverse * r;
    r.w = 0;
    r = uModelViewInverse * r;
    vs_out.rayDir = vec3(r);

    gl_Position = vec4(inPosition, 1.0);
}

fragment:
#version 330

in VS_OUT
{
    vec3 rayDir;
} fs_in;

uniform samplerCube uSrcCubemapSampler;
uniform sampler2D uKernelSampler;
uniform float uBlurHalfArcWidth;
uniform vec3 uLookX;
uniform vec3 uLookY;
uniform vec3 uLookZ;

out vec4 fragColor;

/////////////////////////////////////////////

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main()
{
    vec4 rayDir = vec4(normalize(fs_in.rayDir), 0);

    vec3 colorAccum = vec3(0);
    float arcStep = uBlurHalfArcWidth / 9;
    for (int i = 0; i < 9; i++) {
        float yAngle = (i - 4) * arcStep;
        mat4 yRot = rotationMatrix(uLookY, yAngle);
        for (int j = 0; j < 9; j++) {
            vec2 kernelOffset = vec2((i-4) / 4.0, (j-4)/4.0); // range [-1,1]
            kernelOffset = (kernelOffset * 0.5) + vec2(0.5);
            float weight = texture(uKernelSampler, kernelOffset).r;

            float xAngle = (j - 4) * arcStep;
            mat4 xRot = rotationMatrix(uLookX, xAngle);
            mat4 rot = xRot * yRot;
            vec4 offsetRayDir = rot * rayDir;
            colorAccum += weight * pow(texture(uSrcCubemapSampler, offsetRayDir.xyz).rgb, vec3(2.2));
        }
    }

    colorAccum = pow(colorAccum, vec3(1.0/2.2));
    fragColor = vec4(colorAccum, 1);
}
