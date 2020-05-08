vertex:
#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
}
vs_out;

uniform mat4 uProjectionInverse;
uniform mat4 uViewInverse;

void main()
{
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vs_out.texCoord = inTexCoord;

    vec4 r = vec4(inPosition.xy, 0, 1);
    r = uProjectionInverse * r;
    r.w = 0;
    r = uViewInverse * r;
    vs_out.rayDir = vec3(r);
}

fragment:
#version 330

in VS_OUT
{
    vec2 texCoord;
    vec3 rayDir;
}
fs_in;

out vec4 fragColor;

uniform mat4 uProjectionInverse;
uniform mat4 uViewInverse;
uniform sampler2D uColorSampler;
uniform sampler2D uDepthSampler;
uniform samplerCube uSkyboxSampler;
uniform vec4 uAtmosphericTint;
uniform float uNearRenderDistance;
uniform float uFarRenderDistance;
uniform float uNearPlane;
uniform float uFarPlane;
uniform float uGroundFogPlaneY;
uniform float uGroundFogDistanceUntilOpaque;
uniform vec4 uGroundFogPlaneColor;
uniform vec3 uCameraPosition;

vec3 getFragmentWorldPosition() {
    // https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
    float depth = texture(uDepthSampler, fs_in.texCoord).r;
    float z = depth * 2.0 - 1.0;
    vec4 clipSpacePosition = vec4(fs_in.texCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = uProjectionInverse * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = uViewInverse * viewSpacePosition;
    return worldSpacePosition.xyz;
}

vec3 rayPlaneIntersect(vec3 planeOrigin, vec3 planeNormal, vec3 rayOrigin, vec3 rayDir)
{
    float distance = dot(planeNormal, planeOrigin - rayOrigin) / dot(planeNormal, rayDir);
    return rayOrigin + (rayDir * distance);
}

float rayPlaneDistance(vec3 planeOrigin, vec3 planeNormal, vec3 rayOrigin, vec3 rayDir)
{
    return dot(planeNormal, planeOrigin - rayOrigin) / dot(planeNormal, rayDir);
}

float calcFogContribution(float sceneDepth, vec3 rayDir)
{
    vec3 fogPlaneTopOrigin = vec3(0, uGroundFogPlaneY, 0);
    vec3 fogPlaneNormal = vec3(0, 1, 0);
    float distanceToFogPlane = rayPlaneDistance(fogPlaneTopOrigin, fogPlaneNormal, uCameraPosition, rayDir);
    float contribution = 0;

    // we know the fog plane is horizontal
    if (uCameraPosition.y > uGroundFogPlaneY) {
        // camera above the fog halfspace
        if (rayDir.y >= 0) {
            // ray will never intersect the fog volume
        } else {
            contribution = clamp( (sceneDepth - distanceToFogPlane) / uGroundFogDistanceUntilOpaque, 0.0, 1.0);
        }
    } else {
        // camera inside the fog halfspace
        if (rayDir.y > 0) {
            // ray looking up, will intersect fog plane
            contribution = clamp(min(sceneDepth, distanceToFogPlane) / uGroundFogDistanceUntilOpaque, 0.0, 1.0);
        } else {
            // ray will never intersect fog plane
            contribution = clamp(sceneDepth / uGroundFogDistanceUntilOpaque, 0.0, 1.0);
        }
    }

    return contribution;
}

void main()
{
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 sceneColor = texture(uColorSampler, fs_in.texCoord).rgb;
    vec3 skyboxColor = texture(uSkyboxSampler, rayDir).rgb;
    vec3 fragmentWorldPosition = getFragmentWorldPosition();
    float sceneDepth = distance(uCameraPosition, fragmentWorldPosition);

    // // apply distance-based atmospheric tint
    // float distanceContribution = smoothstep(0, uFarRenderDistance, sceneDepth);
    // sceneColor = mix(sceneColor, uAtmosphericTint.rgb, distanceContribution * uAtmosphericTint.a);


    // appply ground fog to the scene color
    float groundFogContribution = calcFogContribution(sceneDepth, rayDir);
    sceneColor = mix(sceneColor, uGroundFogPlaneColor.rgb, uGroundFogPlaneColor.a * groundFogContribution);

    // prevent "popping" by fading in from skybox color to scene color
    float distanceFogContribution = smoothstep(uNearRenderDistance, uFarRenderDistance, sceneDepth);
    sceneColor = mix(sceneColor, skyboxColor, distanceFogContribution);


    fragColor = vec4(sceneColor, 1);
}
