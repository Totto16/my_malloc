

membench_lib = library('membench', files('membench.c'))

membench_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: membench_lib,
)

allocator_tests_dep = declare_dependency(
    include_directories: include_directories('.'),
    sources: files('allocator_tests.c'),
)
