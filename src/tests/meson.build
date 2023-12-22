test_deps = deps

test_deps += dependency('gtest')

test_src = files('entry.cpp')

test_files = [
    'call_before_initializing.cpp',
    'double_destroy.cpp',
    'double_free.cpp',
    'initialize_error.cpp',
    'normal_operations.cpp',
    'realloc_before_initializing.cpp',
    'realloc_edge_cases.cpp',
    'realloc_freed_block.cpp',
    'realloc_operations.cpp',
]




foreach file : test_files
    file_name = file.split('.')[-2]
    malloc_test = executable(
        'malloc_tests' + file_name,
        test_src,
        files(file),
        dependencies: [test_deps, malloc_normal_dep],
    )
    test(
        'malloc' + file_name,
        malloc_test,
        protocol: 'gtest',
        is_parallel: true,
    )


endforeach


