# build the main executable
hello_app_sources = [
    'main.cpp',
    '../common/cubemap_blur.cpp',
    '../../include/imgui/imgui.cpp',
    '../../include/imgui/imgui_draw.cpp',
    '../../include/imgui/imgui_widgets.cpp',
    '../../include/imgui/imgui_impl_glfw.cpp',
    '../../include/imgui/imgui_impl_opengl3.cpp'
]

executable('hello_mc',
    sources: hello_app_sources,
    include_directories: ['../../include','../../'],
    link_with: mc_lib,
    dependencies: dependencies)
