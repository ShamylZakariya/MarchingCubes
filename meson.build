project('MarchingCubes', 'cpp', version: '0.1', default_options : [
    'c_std=c11',
    'cpp_std=c++17'])


# hide some warnings
ignored_warnings = [
    '-Wno-unknown-pragmas',
    '-Wno-documentation'
]

cxx = meson.get_compiler('cpp')
add_project_arguments(cxx.get_supported_arguments(ignored_warnings), language: 'cpp')

# load required deps; note that on macOS we need to add 'gl'
dependencies = [
    dependency('epoxy'),
    dependency('glfw3'),
    dependency('glm'),
    dependency('threads'),
]

if build_machine.system() == 'darwin'
    dependencies += [
        dependency('gl')
    ]
endif

include_dirs = [
    'include'
]

# build the mc_lib marching cubes library
subdir('mc')


# build the main executable
app_sources = [
    'main.cpp',
    'include/imgui/imgui.cpp',
    'include/imgui/imgui_draw.cpp',
    'include/imgui/imgui_widgets.cpp',
    'include/imgui/imgui_impl_glfw.cpp',
    'include/imgui/imgui_impl_opengl3.cpp'
]

executable('MarchingCubes',
    sources: app_sources,
    include_directories: include_dirs,
    link_with: mc_lib,
    dependencies: dependencies)

## build/register unit tests
subdir('tests')


