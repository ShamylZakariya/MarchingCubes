vec3 rayPlaneIntersect(vec3 planeOrigin, vec3 planeNormal, vec3 rayOrigin, vec3 rayDir)
{
    float distance = dot(planeNormal, planeOrigin - rayOrigin) / dot(planeNormal, rayDir);
    return rayOrigin + (rayDir * distance);
}

float rayPlaneDistance(vec3 planeOrigin, vec3 planeNormal, vec3 rayOrigin, vec3 rayDir)
{
    return dot(planeNormal, planeOrigin - rayOrigin) / dot(planeNormal, rayDir);
}

float getFogPlaneContribution(float groundFogMaxY, float groundFogDensityIncreasePerUnit, vec3 cameraPosition, float float sceneDepth, vec3 fragmentWorldPosition, vec3 rayDir)
{
    vec3 fogPlaneTopOrigin = vec3(0, groundFogMaxY, 0);
    vec3 fogPlaneNormal = vec3(0, 1, 0);
    float distanceToTop = rayPlaneDistance(fogPlaneTopOrigin, fogPlaneNormal, cameraPosition, rayDir);

    float rayStartDensity = 0;
    float rayEndDensity = groundFogDensityIncreasePerUnit * max((groundFogMaxY - fragmentWorldPosition.y), 0.0);
    float rayTraversalDistance = 0;

    // The fog plane is horizontal so testing if camera is in the fog volume is just a check on y
    if (cameraPosition.y > fogPlaneTopOrigin.y) {
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
        rayStartDensity = groundFogDensityIncreasePerUnit * (groundFogMaxY - cameraPosition.y);
        if (rayDir.y > 0) {
            // ray looking up, will intersect fog plane
            rayTraversalDistance = min(sceneDepth, distanceToTop);
        } else {
            // ray looking down, will never intersect fog plane
            rayTraversalDistance = sceneDepth;
        }e
    }

    float c = (0.5 * rayTraversalDistance * (rayEndDensity - rayStartDensity)) + (rayTraversalDistance * rayStartDensity);
    return clamp(c, 0.0, 1.0);
}