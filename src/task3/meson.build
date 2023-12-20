task3_deps = deps

task3_deps += utils_dep
task3_deps += membench_dep


executable(
    'free_list_thread_local',
    files('../task1/my_malloc.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_MALLOC=0', '-D_PER_THREAD_ALLOCATOR=1'],
)

executable(
    'best_fit_thread_local',
    files('../task2/my_malloc.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_MALLOC=1', '-D_PER_THREAD_ALLOCATOR=1'],
)

executable(
    'best_fit_without_bitfield',
    files('../task2/my_malloc.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_MALLOC=1', '-D_PER_THREAD_ALLOCATOR=1', '-D_USE_BIFIELDS=0'],
)

executable(
    'best_fit_with_double_pointers',
    files('../task2/my_malloc_with_pointers.c', 'executable.c'),
    dependencies: task3_deps,
    include_directories: inc_dirs,
    c_args: ['-D_MALLOC=1', '-D_PER_THREAD_ALLOCATOR=1'],
)



