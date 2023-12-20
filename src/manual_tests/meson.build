

membench_lib = library('membench', files('membench.c'))

membench_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: membench_lib,
)

allocator_tests_dep = declare_dependency(
    include_directories: include_directories('.'),
    sources: files('allocator_tests.c'),
)


task3_deps = deps

task3_deps += utils_dep
task3_deps += membench_dep




executable(
    'best_fit_thread_local',
    files('../main/my_malloc.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_PER_THREAD_ALLOCATOR=1'],
)

executable(
    'best_fit_without_bitfield',
    files('../main/my_malloc.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_PER_THREAD_ALLOCATOR=1', '-D_USE_BIFIELDS=0'],
)

executable(
    'best_fit_with_double_pointers',
    files('../main/my_malloc_with_pointers.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_PER_THREAD_ALLOCATOR=1'],
)



