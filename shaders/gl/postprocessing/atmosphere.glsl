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
uniform sampler2D uNoiseSampler;
uniform vec4 uAtmosphericTint;
uniform float uNearRenderDistance;
uniform float uFarRenderDistance;
uniform float uNearPlane;
uniform float uFarPlane;
uniform float uGroundFogPlaneY;
uniform float uGroundFogPlaneDensityIncreasePerMeter;
uniform vec4 uGroundFogPlaneColor;
uniform vec3 uCameraPosition;
uniform vec3 uFogWorldOffset;

float noise(in vec3 x)
{
    float z = x.z * 64.0;
    vec2 offz = vec2(0.317, 0.123);
    vec2 uv1 = x.xy + offz * floor(z);
    vec2 uv2 = uv1 + offz;
    return mix(textureLod(uNoiseSampler, uv1, 0.0).x, textureLod(uNoiseSampler, uv2, 0.0).x, fract(z)) - 0.5;
}

float noises( in vec3 p){
	float a = 0.0;
	for(float i=1.0;i<4.0;i++){
		a += noise(p)/i;
		p = p*2.0 + vec3(0.0,a*0.001/i,a*0.0001/i);
	}
	return a;
}

vec3 getFragmentWorldPosition()
{
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

float getFogPlaneContribution(float sceneDepth, vec3 fragmentWorldPosition, vec3 rayDir)
{
    vec3 fogPlaneTopOrigin = vec3(0, uGroundFogPlaneY, 0);
    vec3 fogPlaneNormal = vec3(0, 1, 0);
    float distanceToTop = rayPlaneDistance(fogPlaneTopOrigin, fogPlaneNormal, uCameraPosition, rayDir);

    float rayStartDensity = 0;
    float rayEndDensity = uGroundFogPlaneDensityIncreasePerMeter * max((uGroundFogPlaneY - fragmentWorldPosition.y), 0.0);
    float rayTraversalDistance = 0;

    // The fog plane is horizontal so testing if camera is in the fog volume is just a check on y
    if (uCameraPosition.y > fogPlaneTopOrigin.y) {
        // camera above the fog halfspace
        if (rayDir.y >= 0) {
            // ray looking up, will never intersect the fog volume
            rayEndDensity = 0;
        } else {
            // ray looking down, transits through fog volume top
            rayTraversalDistance = sceneDepth - distanceToTop;
        }
    } else {
        // camera inside the fog halfspace
        rayStartDensity = uGroundFogPlaneDensityIncreasePerMeter * (uGroundFogPlaneY - uCameraPosition.y);
        if (rayDir.y > 0) {
            // ray looking up, will intersect fog plane
            rayTraversalDistance = min(sceneDepth, distanceToTop);
        } else {
            // ray looking down, will never intersect fog plane
            rayTraversalDistance = sceneDepth;
        }
    }

    float c = (0.5 * rayTraversalDistance * (rayEndDensity - rayStartDensity)) + (rayTraversalDistance * rayStartDensity);
    return clamp(c, 0.0, 1.0);
}

vec4 getFogContribution(float sceneDepth, vec3 fragmentWorldPosition, vec3 rayDir, vec3 skyboxColor)
{
    // march backwards from distant to close


    float dist = uFarRenderDistance;
    //float dist = min(uFarRenderDistance, sceneDepth);


    vec3 marchPosition = uCameraPosition + rayDir * dist;
    int steps = 30;
    vec3 marchDir = uCameraPosition - marchPosition;
    float rayLength = length(marchDir);
    vec3 marchIncrement = marchDir / steps;
    float lengthIncrement = rayLength / steps;

    vec3 color = skyboxColor;
    float totalContribution = 0;
    for (int i = 0; i < steps; i++) {
        // sample fog at march position
        if (rayLength < sceneDepth) {
            vec3 samplePosition = marchPosition + uFogWorldOffset;
            float c = noises(samplePosition * 0.00025);
            c = (c + 1) * 0.5; // remap from [-1,1] to [0,1]
            c = smoothstep(0.35, 1, c);

            // fade out as it approaches the fog plane top
            float dy = clamp((uGroundFogPlaneY - samplePosition.y) / uGroundFogPlaneY, 0, 1);
            c *= dy;

            if (c > 0) {
                totalContribution += c;
                vec3 contribution = mix(uGroundFogPlaneColor.rgb, skyboxColor, rayLength / uFarRenderDistance);
                color = mix(color, contribution, c);
            }
        }
        marchPosition += marchIncrement;
        rayLength -= lengthIncrement;
    }

    return vec4(color, min(totalContribution * uGroundFogPlaneColor.a, 1));
}

void main()
{
    vec3 rayDir = normalize(fs_in.rayDir);
    vec3 sceneColor = texture(uColorSampler, fs_in.texCoord).rgb;
    vec3 skyboxColor = texture(uSkyboxSampler, rayDir).rgb;
    vec3 fragmentWorldPosition = getFragmentWorldPosition();
    float sceneDepth = distance(uCameraPosition, fragmentWorldPosition);


    vec4 fog = getFogContribution(sceneDepth, fragmentWorldPosition, rayDir, skyboxColor);

    // // apply distance-based atmospheric tint
    // float distanceContribution = smoothstep(0, uFarRenderDistance, sceneDepth);
    // vec3 groundFogColor = mix(uGroundFogPlaneColor.rgb, uAtmosphericTint.rgb, distanceContribution * uAtmosphericTint.a);

    // prevent "popping" by fading in from skybox color to scene color
    float distanceFogContribution = smoothstep(uNearRenderDistance, uFarRenderDistance, sceneDepth);
    sceneColor = mix(sceneColor, skyboxColor, distanceFogContribution);

    sceneColor = mix(sceneColor, fog.rgb, fog.a);
    fragColor = vec4(sceneColor, 1);

    // // appply ground fog to the scene color
    // float groundFogContribution = getFogPlaneContribution(sceneDepth, fragmentWorldPosition, rayDir);
    // float rayHorizonContribution = 1 - abs(rayDir.y);
    // float localDistanceFogColorMix = distanceFogContribution * rayHorizonContribution;
    // groundFogColor = mix(groundFogColor, skyboxColor, localDistanceFogColorMix);
    // float groundFogAlpha = mix(uGroundFogPlaneColor.a, 1, localDistanceFogColorMix);

    // sceneColor = mix(sceneColor, groundFogColor.rgb, groundFogAlpha * groundFogContribution);
    // fragColor = vec4(sceneColor, 1);
}
