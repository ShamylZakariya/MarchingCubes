unit_test_sources = [
    ['test_samplers', 'test_samplers.cpp'],
    ['test_octree', 'test_octree.cpp'],
]

# build an exe per test source
foreach test_source : unit_test_sources
    test_name = test_source[0]
    test_source = test_source[1]
    test_exe = executable(test_name,
        sources: test_source,
        include_directories: ['../../include', '../..'],
        link_with: mc_lib,
        dependencies: dependencies)

    test('unit', test_exe)
endforeach