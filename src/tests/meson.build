test_deps = deps

test_deps += dependency('gtest')


test_src = files('entry.cpp', 'malloc.cpp')



malloc_tests = executable(
    'malloc_tests',
    test_src,
    dependencies: [test_deps, malloc_normal_dep],
)


test('malloc', malloc_tests, protocol: 'gtest')
