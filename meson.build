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
subdir('demos')


