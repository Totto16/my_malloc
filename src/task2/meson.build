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
        '-D_WITH_REALLOC',
        common_args,
    ],
)

malloc_normal_lib = library(
    'malloc_normal',
    files('my_malloc_with_pointers.c'),
    dependencies: utils_dep,
    c_args: [
        '-D_WITH_REALLOC',
    ],
)

malloc_normal_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: malloc_normal_lib,
)

executable(
    'tests_with_double_pointers_single_threaded',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_ALLOCATOR_NOT_MT_SAVE',
        '-D_WITH_REALLOC',
        common_args,
    ],
)


executable(
    'tests_with_double_pointers_thread_local',
    files('executable.c', 'my_malloc_with_pointers.c'),
    dependencies: task2_deps,
    include_directories: inc_dirs,
    c_args: [
        '-D_PER_THREAD_ALLOCATOR=1',
        '-D_WITH_REALLOC',
        common_args,
    ],
)
