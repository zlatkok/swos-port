additionalSwitches = ['/wd5033', '-DNDEBUG', '/UDEBUG', '/U_DEBUG']

subdir('gen-lookup')

# gather source files
c = run_command([python, '-c', getFilesCommand.format('.', '*.cpp')], check: true)
rawSources = c.stdout().strip().split('\n')

sources = []
foreach source : rawSources
    if not source.endswith('stdinc.cpp')
        sources += source
    endif
endforeach

# gather include directories
c = run_command([python, '-c', getDirsCommand.format('src')], check: true)
rawDirs = c.stdout().strip().split('\n')

ida2asmIncludeDirs = []
foreach dir : rawDirs
    ida2asmIncludeDirs += dir
endforeach

ida2asm = executable('ida2asm', sources, tokenLookup,
    include_directories: [thirdPartyIncludeDirs, ida2asmIncludeDirs],
    cpp_args: additionalSwitches,
    cpp_pch: 'src' / 'stdinc.h',
    override_options: ['optimization=3', 'debug=false', 'b_vscrt=md'],
)
