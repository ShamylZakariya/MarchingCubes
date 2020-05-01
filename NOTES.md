# NEXT:
- material application is wonky; you can see it where arches meet the ground
- arches show more stairstepping than I'm happy with.

# POST PROCESSING
- look into a raymarched cloud/fog https://www.shadertoy.com/view/Msf3zX
- how can I write to the depth buffer on the final composite pass? I should like for the wireframes to intersect
    - gl_FragDepth
    https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL
- according to http://www.songho.ca/opengl/gl_fbo.html it's faster to create ONE fbo and switch between attachments than to switch FBOs.


# TODO:
- Previously, quitting app while a march was in progress caused a crash; we're usig a new threadpool and this may not be reproducible any more.