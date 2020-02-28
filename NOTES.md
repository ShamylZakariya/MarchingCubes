# NEXT:
- Automatic camera tracking through waypoints
- If you quit while a segment is marching we get a crash - so we need some kind of flag on quit that says "a quit is requested, but we have to wait until it's safe"
- Each Volume should be given the thread pool to use, not create its own - however, this requires that we have some way to wait on a batch, not on all jobs. Like:
    auto batch = pool->startBatch();
    batch->enqueue()
    ...
    batch->wait()