project('MarchingCubes', 'cpp', version: '0.1', default_options : [
    'c_std=c11',
    'cpp_std=c++17'])

cxx = meson.get_compiler('cpp')

ignored_warnings = [
    '-Wno-unknown-pragmas',
    '-Wno-documentation'
]

add_project_arguments(cxx.get_supported_arguments(ignored_warnings), language: 'cpp')

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

sources = [
    'src/marching_cubes.cpp',
    'src/storage.cpp',
    'src/triangle_soup.cpp',
    'src/volume.cpp',
    'src/util.cpp'
]

## executable

app_sources = [
    'src/main.cpp'
]

executable('MarchingCubes',
    sources: sources + app_sources,
    include_directories: include_dirs,
    dependencies: dependencies)

## unit tests

unit_test_sources = [
    'tests/unit/tests.cpp',
]

unit_test_exe = executable('UnitTests',
    sources: sources + unit_test_sources,
    include_directories: include_dirs,
    dependencies: dependencies)

test('unit', unit_test_exe)


