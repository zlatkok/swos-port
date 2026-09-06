kSwosCppFiles = [
    'swos-01.cpp',
    'swos-02.cpp',
    'swos-03.cpp',
    'swos-04.cpp',
    'swos-05.cpp',
    'swos-06.cpp',
    'swos-07.cpp',
    'swos-08.cpp',
]

# Generate the VM-backed C++ implementation used by the current Visual Studio
# projects. ida2asm is deliberately built as an optimized target because its
# Debug diagnostics are unsuitable for normal game and test builds.
swosCpp = custom_target('swos-cpp',
    input: ['swos.asm', 'symbols.txt'],
    output: ['swossym.h'] + kSwosCppFiles + ['vm.cpp', 'vm.h', 'defs.h'],
    command: [
        ida2asm,
        '@INPUT0@',
        '@OUTDIR@' / 'swos.cpp',
        '@INPUT1@',
        '@OUTPUT0@',
        'c++',
        kSwosCppFiles.length().to_string(),
        '--extra-memory-size=60000',
        '--disable-alignment-checks',
    ],
)

swosGeneratedSources = []
foreach i : range(1, 10)
    swosGeneratedSources += swosCpp[i]
endforeach
