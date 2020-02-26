# build the main executable
terrain_app_sources = [
    'main.cpp',
    'FastNoise.cpp',
    '../common/cubemap_blur.cpp',
    '../../include/imgui/imgui.cpp',
    '../../include/imgui/imgui_draw.cpp',
    '../../include/imgui/imgui_widgets.cpp',
    '../../include/imgui/imgui_impl_glfw.cpp',
    '../../include/imgui/imgui_impl_opengl3.cpp'
]

executable('terrain',
    sources: terrain_app_sources,
    include_directories: ['../../include','../../'],
    link_with: mc_lib,
    dependencies: dependencies)
