# build the main executable
app_sources = [
    'main.cpp',
    'cubemap_blur.cpp',
    '../include/imgui/imgui.cpp',
    '../include/imgui/imgui_draw.cpp',
    '../include/imgui/imgui_widgets.cpp',
    '../include/imgui/imgui_impl_glfw.cpp',
    '../include/imgui/imgui_impl_opengl3.cpp'
]

executable('MarchingCubes',
    sources: app_sources,
    include_directories: ['../include','../'],
    link_with: mc_lib,
    dependencies: dependencies)
