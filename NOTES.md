# TODO:

- room for big optimization: an IVolumeSampler should be able to say it *completely* overwrites
all the values in an OctreeVolume::Node, thereby guaranteeing all samples in it to be 1 or 0; this
can allow us to discard any other samplers from contributing. If the sampler is Subtractive, it also means
we can discard that node completely.
    - This may have edge case implications, where such a node touches another
node for which the situation does not hold?
        - no; the full occupation case only happens deep inside a sampler and doesn't intersect its edges. we need to ensure this takes fuzziness into account. This would require fuzziness to be added to the test() function.

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
