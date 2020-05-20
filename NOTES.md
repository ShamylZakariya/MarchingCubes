# Post Processing
- look into a raymarched cloud/fog https://www.shadertoy.com/view/Msf3zX
    - start with a static fog plane, this will help me learn the basics... just needs two plane equations, upper and lower bounds. Trace ray in worldspace through these two planes.
    - drop the "fade in" in terrain.glsl and do that in the atmosphere shader
        - drop the skycubemap render pass
        - render the sky as post processing in the atmosphere shader

- Build a single filter which:
    - color grades/paletizes
    - renders raymarched clouds

- for the low-fi effect to look more legit i need to palettize texture sampling too... need to use nearest interpolation, palettize colors, etc.

# Round World
Implement an animal crossing-style spherical world. Easily done in terain.glsl shader. Does of course break the fog "plane" post-processing shader, since that will have to become a fog sphere.

# Volumetric Noise-based Fog
This is going to be expensive, so I will need some way to work on a downsampled depth channel, and then linearly upscale the computed fog.

We have a distance color and a near color. So should fog march backwards, from back to front?

Make the simple approach first (no downscaling) verify it works and looks decent. Then worry about how to get performance.

# Continous Terrain
- 3x3 (or 5x5) grid, with viewer always in center patch
- on transit across path boundaries, recycle the old patches into new positions, with a unique id based on grid position to seed the rng
- need to do view frustum culling -- attempting to draw too many chunks
- need a priority queue to serialize the marching of volumes. sort by dot product of aabb.center against look dir.