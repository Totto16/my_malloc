task2_deps = deps

task2_deps += utils_dep
task2_deps += membench_dep
task2_deps += allocator_tests_dep

executable(
    'tests',
    files('executable.c', 'my_malloc.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: ['-D_USE_BIFIELDS=1'],
)

executable(
    'tests_without_bitfield',
    files('executable.c', 'my_malloc.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: ['-D_USE_BIFIELDS=0'],
)

executable(
    'tests_with_double_pointers',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_VALIDATE_BLOCKS=0',
        '-D_PRINT_DEBUG=0',
        '-D_IGNORE_SECURITY_CHECKS',
    ],
)

executable(
    'tests_with_double_pointers_debug',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_VALIDATE_BLOCKS=1',
        '-D_PRINT_DEBUG=1',
        '-U_IGNORE_SECURITY_CHECKS',
    ],
)
