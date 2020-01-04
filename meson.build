project('MarchingCubes', 'cpp', default_options : [
    'c_std=c11', 
    'cpp_std=c++17'])

thread_dep = dependency('threads')
epoxy_dep = dependency('epoxy')
glfw_dep = dependency('glfw3')
glm_dep = dependency('glm')

dependencies = [
    epoxy_dep,
    glfw_dep,
    glm_dep,
    thread_dep,
]

include_dirs = [
    'include'
]

sources = [
    'src/marching_cubes.cpp',
    'src/storage.cpp',
    'src/triangle_soup.cpp',
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


