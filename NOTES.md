# NEXT:
- Automatic camera tracking through waypoints
- Each Volume should be given the thread pool to use, not create its own - however, this requires that we have some way to wait on a batch, not on all jobs. Like:
    auto batch = pool->startBatch();
    batch->enqueue()
    ...
    batch->wait()