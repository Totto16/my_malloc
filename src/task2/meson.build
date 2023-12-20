task2_deps = deps

task2_deps += utils_dep
task2_deps += membench_dep
task2_deps += allocator_tests_dep

common_args = ['-D_WANT_TO_USE_VALGRIND']

executable(
    'tests',
    files('executable.c', 'my_malloc.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: ['-D_USE_BIFIELDS=1', common_args],
)

executable(
    'tests_without_bitfield',
    files('executable.c', 'my_malloc.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: ['-D_USE_BIFIELDS=0', common_args],
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
        common_args,
    ],
)

executable(
    'tests_with_double_pointers_single_threaded',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_VALIDATE_BLOCKS=0',
        '-D_PRINT_DEBUG=0',
        '-D_IGNORE_SECURITY_CHECKS',
        '-D_ALLOCATOR_NOT_MT_SAVE',
        common_args,
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
        common_args,
    ],
)


executable(
    'tests_with_double_pointers_thread_local',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_VALIDATE_BLOCKS=0',
        '-D_PRINT_DEBUG=0',
        '-D_IGNORE_SECURITY_CHECKS',
        '-D_PER_THREAD_ALLOCATOR=1',
        common_args,
    ],
)
