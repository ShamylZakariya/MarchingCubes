# NEXT:
I need to implement non blocking marching. I can do it a few ways:
    1) Make an atomic counter for each job that will march, and have those jobs decrement the counter on completion. The main loop can poll the state of that counter; some other state (enum? bool) can say we've gone from empty|marching|marchComplete|uploadedGeometry and can make appropriate transitions on main thread for GL calls
    2) Make a main-thread-queue, (mc::util::MainThreadQueue) and have it be pumped in Step() (if I'm clever, maybe there's a way to hook it directly into glfw's callbacks?). Instead of a blocking wait on the thread worker, create a thread which calls wait, and when complete adds a callback to the global main thread queue to call finish() on all the triangle consumers. We need to ensure that thread is not stack local! else it will dies when the async march function completes.

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
