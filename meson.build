project('MarchingCubes', 'cpp', default_options : [
    'c_std=c11', 
    'cpp_std=c++17'])

thread_dep = dependency('threads')
epoxy_dep = dependency('epoxy')
glfw_dep = dependency('glfw3')
glm_dep = dependency('glm')

include_dirs = [
    'include'
]

sources = [
    'src/main.cpp',
    'src/marching_cubes.cpp',
    'src/storage.cpp',
    'src/triangle_soup.cpp',
    'src/util.cpp'
]

dependencies = [
    thread_dep,
    epoxy_dep,
    glfw_dep,
    glm_dep
]

executable('MarchingCubes', 
    sources: sources, 
    include_directories: include_dirs,
    dependencies: dependencies)




