# POST PROCESSING
- look into a raymarched cloud/fog https://www.shadertoy.com/view/Msf3zX
    - start with a static fog plane, this will help me learn the basics... just needs two plane equations, upper and lower bounds. Trace ray in worldspace through these two planes.
    - drop the "fade in" in terrain.glsl and do that in the atmosphere shader
        - drop the skycubemap render pass
        - render the sky as post processing in the atmosphere shader

- Build a single filter which:
    - color grades/paletizes
    - renders raymarched clouds

- for the ffect to look more legit i need to palettize texture sampling too... need to use nearest interpolation, palettize colors, etc.

# ROUND WORLD
Implement an animal crossing-style spherical world. Easily done in terain.glsl shader. Does of course break the fog "plane" post-processing shader, since that will have to become a fog sphere.