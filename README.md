# MarchingCubes
Just a simple marching cubes experiment.

This started as an experiment in writing a multi-threaded marching cubes implementation, and spiralled slowly, inexoribly into an [LV-426](https://alienanthology.fandom.com/wiki/Acheron_(LV-426)) simulator, and *I couldn't be happier*.

![example](README_assets/lv_426.png)

## Building

Marching Cubes depends on [libepoxy](https://github.com/anholt/libepoxy), [glfw3](https://www.glfw.org/) and [glm](https://glm.g-truc.net/)

```bash
#linux
sudo apt-get install libepoxy-dev libglfw3-dev libglm-dev

```

The VSCode project depends on meson building into `build`
```bash
# manually
CC=clang CXX=clang++ meson setup --layout flat build

# or alternately
./setup.sh
```

Then you can open the project in VSCode
```build
code .
```

Note, since the VSCode build task depends on `build/compile_commands.json` it's recommended to run `meson setup build` once before opening the project in VSCode.

From here, the default build task is **ctrl-shift-b** and debugging via **F5** works as expected.

If using `gcc`, use `(gdb) Launch`, and if using `clang`, use `(CodeLLDB) Launch`. The latter requires the `CodeLLDB` extension to be installed.´

## Running

The build specifies two targets, `hello_mc` which is a simple exploratory playground, and `terrain` which is an "infinite" procedural world.