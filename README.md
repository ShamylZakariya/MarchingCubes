# MarchingCubes
Just a simple marching cubes experiment

![example animation](example.gif)

## Building on Linux

### Dependencies

Marching Cubes depends on [libepoxy](https://github.com/anholt/libepoxy), [glfw3](https://www.glfw.org/) and [glm](https://glm.g-truc.net/)

```bash
sudo apt-get install libepoxy-dev libglfw3-dev libglm-dev
```

The VSCode project depends on meson building into `build`
```bash
meson setup build
```

Then you can open the project in VSCode
```build
code .
```

From here, the default build task is `ctrl-shift-b` and debugging via F5 works as expected.