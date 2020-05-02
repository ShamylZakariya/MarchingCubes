# NEXT:
- material application is wonky; you can see it where arches meet the ground
- sometimes crashes on quit because the worker threads are still running. We need to have some kind of termination signal to prevent new jobs from starting when quitting, and to wait for current jobs to finish.

# POST PROCESSING
- look into a raymarched cloud/fog https://www.shadertoy.com/view/Msf3zX