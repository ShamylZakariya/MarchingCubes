# NEXT:
- Automatic camera tracking through waypoints
- If you quit while a segment is marching we get a crash - so we need some kind of flag on quit that says "a quit is requested, but we have to wait until it's safe"
- Each Volume should be given the thread pool to use, not create its own - however, this requires that we have some way to wait on a batch, not on all jobs. Like:
    auto batch = pool->startBatch();
    batch->enqueue()
    ...
    batch->wait()
- We need to, periodically, reset scroll so as to not drift into float precision issues
    if _distanceAlongX > N:
        _distanceAlongX -= N
        for seg in segments:
            seg.model = seg.model * translate(mat4(1), vec3(0,0,-N))

- I should collect/mark on a thread, because it's likely the source of the hiccup at runtime; this depends on the batching thread pool above to be gracefully implemented...