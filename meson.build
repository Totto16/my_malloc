project(
    'my own malloc',
    'c',
    'cpp',
    license: 'MIT',
    meson_version: '>=1.2.0',
    default_options: {
        'buildtype': 'release',
        'optimization': '3',
        'c_std': 'gnu2x',
        'warning_level': '3',
        'werror': 'true',
        'b_ndebug': 'if-release',
    },
    version: '1.0.0',
)


deps = []
compile_flags = []

deps += dependency('threads')

valgrdind_dep = dependency('valgrind', required: false)
if valgrdind_dep.found()
    compile_flags += '-D_WITH_VALGRIND=1'
    deps += valgrdind_dep
endif

subdir('src')

