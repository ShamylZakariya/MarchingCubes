uniform vec3 uLightDir;
uniform vec3 uHorizonColor;
uniform vec3 uSpaceColor;
uniform vec3 uSunColor;
uniform vec3 uSunsetColor;

vec3 sky(vec3 worldDir, float shininess)
{
    float closenessToHorizon = 1 - max(worldDir.y, 0);
    closenessToHorizon *= closenessToHorizon;
    float softness = mix(1, 0.0625, shininess * closenessToHorizon);

    vec3 skydomeColor = mix(uHorizonColor, uSpaceColor, smoothstep(0, 1, worldDir.y));
    skydomeColor = mix(skydomeColor, uSunsetColor, closenessToHorizon * max(dot(vec3(worldDir.x, 0, worldDir.z), uLightDir), 0));

    float sunAmount = max(dot(worldDir, uLightDir), 0.0);
    vec3 sunColor = mix(uSunColor, uSunsetColor, closenessToHorizon);

    vec3 skydomeWithGlare = mix(skydomeColor, sunColor, pow(sunAmount, 10.0 * softness));
    vec3 skydomeWithSun = mix(skydomeWithGlare, vec3(1, 1, 1), pow(sunAmount, 20.0 * softness));

    return skydomeWithSun;
}