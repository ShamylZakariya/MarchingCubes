# NEXT:
- each vertex should have triangle and vertex normals (which means I need to calc all vertex normals)
    - I can then in the vert shader use a crease threshold (dot(triangleNormal,vertexNormal)) to select the normal
    to interpolate in the fragment shader. This would allow for nice smooth surfaces, with creases at edges
    - This means I need to figure out my faster dedicated normal calculator. If each IVolumeSampler can compute
    a normal, I can just use the normal from the last sampler to contribute > 0 to a point in space?
        Consider:
            - add sphere to cube: The cube's normals override the sphere's
            - subtract cube from sphere: Same story, but normals inverted



# TODO:
- should AABBs be immutable data objects? If I do this I can make them cache their corners, which could be a speedup

- What to do about the volume's boundary? Right now we march up to it, but we don't march beyond; we get holes. Is the correct behavior to march one step beyond with a sampler that always returns 0 to get hole patching? Soould that be optional or default?

- Implement non-linear interpolation
```cpp
// polynomial smooth min
// https://www.iquilezles.org/www/articles/smin/smin.htm
static inline float smin(float a, float b, float k)
{
    float h = std::max<float>(k - abs(a - b), 0.0f) / k;
    return std::min<float>(a, b) - h * h * k * (1.0f / 4.0f);
}

// cubic smooth min
// https://www.iquilezles.org/www/articles/smin/smin.htm
static inline float sminCubic(float a, float b, float k)
{
    float h = std::max<float>(k - abs(a - b), 0.0f) / k;
    return std::min<float>(a, b) - h * h * h * k * (1.0f / 6.0f);
}
```
