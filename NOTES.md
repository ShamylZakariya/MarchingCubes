# TODO:
1) Fix the crashes on shutdown
2) Make error handling (glsl compiler errors, etc) NOT blow up the job queues

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