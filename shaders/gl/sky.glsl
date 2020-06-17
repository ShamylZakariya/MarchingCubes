vec3 sky(vec3 worldDir, float shininess) {
    vec3 kLightDir = normalize(vec3(1,1,0));
    vec3 kHorizonColor = vec3(0.6, 0.8, 1);
    vec3 kSpaceColor = vec3(0, 0.07, 0.4);
    vec3 kSunColor = vec3(1, 0.9, 0.7);
    float powScale = mix(1, 0.25, shininess);

    vec3 c0 = mix(kHorizonColor, kSpaceColor, smoothstep(0, 1, worldDir.y));

    float sunAmount = max(dot(worldDir, kLightDir), 0.0);
    vec3 c1 = mix(c0, kSunColor, pow(sunAmount, 8.0 * powScale));
    c1 = mix(c1, vec3(1,1,1), pow(sunAmount, 20.0 * powScale));

    return c1;
}