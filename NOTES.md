# Bugs
- The cube has rendering errors; looks like it's not touching all the nodes it needs to touch to be marched correctly
    - using a crude bounding box test we get correct march and node selection, so we know it's an error in the "smart" test() implementation
    - finding the closest point of the aabb doesn't work
        - possible to find closest point of aabb to the closest point of the plane?
            - YES. But it's greedy and causes false positives which need to be filtered by coarse bounds checks
    - see
        - http://www.jkh.me/files/tutorials/Separating%20Axis%20Theorem%20for%20Oriented%20Bounding%20Boxes.pdf
        - https://www.gamedev.net/forums/topic/628444-collision-detection-between-non-axis-aligned-rectangular-prisms/

# TODO:

- What to do about the volume's boundary? Right now we march up to it, but we don't march beyond; we get holes. Is the correct behavior to march one step beyond with a sampler that always returns 0 to get hole patching? Soould that be optional or default?

- Implement SDF smoothing:
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
