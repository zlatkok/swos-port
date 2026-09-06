# gather all the source files
c = run_command([python, '-c', getFilesCommand.format('.', '*.cpp')], check: true)
rawSources = c.stdout().strip().split('\n')

currentSrcDir = meson.current_source_dir()
sourceFiles = []

# we'll need this dict later to pick & choose project files for the tester
sourceFilenamesToFiles = {}

foreach source : rawSources
    if not source.endswith('stdinc.cpp')
        file = files(source)
        sourceFiles += file
        sourceFilenamesToFiles += {source: file}
    endif
endforeach

# add all subdirectories as includes
c = run_command([python, '-c', getDirsCommand.format('.')], check: true)
rawDirs = c.stdout().strip().split('\n')

srcIncludeDirs = ['.']
foreach dir : rawDirs
    srcIncludeDirs += dir
endforeach

srcIncludeDirs = include_directories(srcIncludeDirs)

subdir('menus')

animationTables = custom_target('animation-tables',
    input: [
        'game' / 'animation' / 'animationTables.in',
        'game' / 'animation' / 'generateAnimationTables.py',
    ],
    output: ['animationTables.h', 'animationTables.cpp'],
    depend_files: files(
        'game' / 'animation' / 'originalSwosAnimationTables.py',
        'sprites' / 'sprites.h',
        'swos' / 'swos.h',
    ),
    command: [
        python,
        '@INPUT1@',
        '@INPUT0@',
        '@OUTPUT0@',
        '@OUTPUT1@',
        '--sprites', meson.current_source_dir() / 'sprites' / 'sprites.h',
        '--swos', meson.current_source_dir() / 'swos' / 'swos.h',
    ],
)
