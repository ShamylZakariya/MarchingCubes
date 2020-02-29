# NEXT:
- I should collect/mark on a thread, because it's likely the source of the hiccup at runtime; this depends on the batching thread pool above to be gracefully implemented...

# TODO:
- Previously, quitting app while a march was in progress caused a crash; we're usig a new threadpool and this may not be reproducible any more.