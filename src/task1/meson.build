task1_deps = deps

task1_deps += utils_dep
task1_deps += membench_dep
task1_deps += allocator_tests_dep

executable(
    'tests',
    files('executable.c', 'my_malloc.c'),
    dependencies: task1_deps,
    include_directories: inc_dirs,
    c_args: ['-D_PER_THREAD_ALLOCATOR=0'],
)

# TODO: this isn't working correctly, since membench crashes !
executable(
    'thread_local_tests',
    files('executable.c', 'my_malloc.c'),
    dependencies: task1_deps,
    include_directories: inc_dirs,
    c_args: ['-D_PER_THREAD_ALLOCATOR=1'],
)
