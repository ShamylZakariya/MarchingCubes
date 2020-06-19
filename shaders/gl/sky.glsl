vec3 sky(vec3 worldDir, float shininess) {
    vec3 kLightDir = normalize(vec3(0.7,0.3,0));
    vec3 kHorizonColor = vec3(0.6, 0.8, 1);
    vec3 kSpaceColor = vec3(0, 0.07, 0.4);
    vec3 kSunColor = vec3(1, 0.9, 0.7);
    vec3 kSunsetColor = vec3(1, 0.5, 0.67);
    float closenessToHorizon = 1 - max(worldDir.y, 0);
    closenessToHorizon *= closenessToHorizon;
    float softness = mix(1, 0.0625, shininess * closenessToHorizon);


    vec3 skydomeColor = mix(kHorizonColor, kSpaceColor, smoothstep(0, 1, worldDir.y));
    skydomeColor = mix(skydomeColor, kSunsetColor, closenessToHorizon * max(dot(vec3(worldDir.x, 0, worldDir.z), kLightDir), 0));

    float sunAmount = max(dot(worldDir, kLightDir), 0.0);
    vec3 sunColor = mix(kSunColor, kSunsetColor, closenessToHorizon);

    vec3 skydomeWithGlare = mix(skydomeColor, sunColor, pow(sunAmount, 10.0 * softness));
    vec3 skydomeWithSun = mix(skydomeWithGlare, vec3(1,1,1), pow(sunAmount, 20.0 * softness));

    return skydomeWithSun;
}