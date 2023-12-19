project(
    'my own malloc',
    'c',
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

deps += dependency('threads')

subdir('src')
