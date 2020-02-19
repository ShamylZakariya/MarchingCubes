# NEXT:
    - my crease threshold isn't quite right - I should see smooth transition from flat joins to the crease, but I see instead fully flat triangles
    - normalSampler kind of works, but has errors in CompoundShapeDemo when dealing with the subtractiveplane

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
