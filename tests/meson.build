subdir('sdl-address-table-fetcher')

custom_target('copy-fetcher-dll',
    input: sdlAddressTableFetcher,
    output: fetcherDllName,
    command: [python, '-c', 'import shutil;shutil.copy2(r"@INPUT@", r"@OUTPUT@")'],
    build_by_default: true,
)

c = run_command([python, '-c', getFilesCommand.format('src', '*.cpp')], check: true)
rawSources = c.stdout().strip().split('\n')

testSources = []
foreach source : rawSources
    if source != 'src/stdinc.cpp'
        testSources += source
    endif
endforeach

c = run_command([python, '-c', getDirsCommand.format('src')], check: true)
testIncludeDirs = include_directories(c.stdout().strip().split('\n'))

# The test project mirrors the Visual Studio project: it links most production
# sources, replacing platform-facing modules with mocks.
testExcludedProjectSources = [
    'audio/audio.cpp',
    'audio/music.cpp',
    'crash.cpp',
    'debug/dump.cpp',
    'files/file.cpp',
    'main.cpp',
    'options/options.cpp',
    'sprites/renderSprites.cpp',
    'util/log.cpp',
    'util/util.cpp',
    'video/overlay.cpp',
    'video/render.cpp',
    'video/timer.cpp',
    'video/windowManager.cpp',
]

testProjectSources = []
foreach filename, source : sourceFilenamesToFiles
    if filename not in testExcludedProjectSources
        testProjectSources += source
    endif
endforeach

testCppArgs = commonCppArgs + [
    '-DSWOS_TEST',
    '-DVIRTUAL_JOYPAD',
    '-DDECLSPEC=',
    '-DTEST_DATA_DIR=R"(@0@)"'.format(meson.current_source_dir() / 'data'),
    fetcherDefine,
]

if host_machine.cpu_family() == 'x86_64'
    testCppArgs += '-DDISABLE_ALIGNMENT_CHECKS=1'
endif

testsExecutable = executable('tests',
    testProjectSources,
    testSources,
    swosGeneratedSources,
    swosCpp[0],
    menuFiles,
    animationTables[0],
    assetHeaders,
    include_directories: [testIncludeDirs, thirdPartyIncludeDirs, srcIncludeDirs,
        'sdl-address-table-fetcher' / 'src'],
    link_with: sdlAddressTableFetcher,
    dependencies: commonLibraries,
    cpp_args: testCppArgs,
    cpp_pch: '..' / 'src' / 'stdinc.h',
)
