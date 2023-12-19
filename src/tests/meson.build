

membench_lib = library('membench', files('membench.c'))
allocator_tests_lib = library('allocator_tests', files('allocator_tests.h'))

membench_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: membench_lib,
)

allocator_tests_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: allocator_tests_lib,
    extra_files: files('allocator_tests.c'),
)
