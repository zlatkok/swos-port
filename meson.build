project('swos-port', 'cpp',
    default_options: [
        'cpp_std=c++latest',
        'warning_level=2',
        'b_vscrt=from_buildtype',
    ],
    version: '1.0',
    meson_version: '>=1.3.0',
)

if host_machine.system() != 'windows'
    error('The desktop swos-port build currently supports Windows only')
endif

fs = import('fs')
pymod = import('python')
python = pymod.find_installation('python', required: true)
assert(python.language_version().version_compare('>=3.8'), 'Python 3.8 or newer is required')

isDebugBuild = get_option('buildtype') == 'debug'
libraryConfiguration = isDebugBuild ? 'debug' : 'release'
libraryPlatform = host_machine.cpu_family() == 'x86_64' ? 'win64' : 'win32'

commonCppArgs = [
    '/Zc:__cplusplus',
    '/wd4731',
    '/FIoptional',
    '-DNOMINMAX',
    '-DSWOS_VM',
]
if isDebugBuild
    commonCppArgs += ['-DDEBUG', '/UNDEBUG']
else
    commonCppArgs += '-DNDEBUG'
endif

# Helpers used at configure time to make project/source lists visible in IDEs.
getFilesCommand = 'import os;import pathlib;' + \
    'print(*(os.path.relpath(f).replace("\\\\","/") for f in pathlib.Path("@0@").rglob("@1@")),sep="\\n")'
getDirsCommand = 'import os;' + \
    'print(*(os.path.relpath(d).replace("\\\\","/") for d,_,_ in os.walk("@0@")),sep="\\n")'

thirdPartyIncludeDirs = include_directories([
    '3rd-party' / 'SDL2' / 'include',
    '3rd-party' / 'SDL2_image' / 'include',
    '3rd-party' / 'SDL_mixer' / 'include',
    '3rd-party' / 'CrashRpt' / 'include',
    '3rd-party' / 'minizip' / 'include',
    '3rd-party' / 'zlib-ng' / 'include',
    '3rd-party' / 'SimpleIni',
    '3rd-party' / 'dirent',
], is_system: true)

cppCompiler = meson.get_compiler('cpp')
archLibRoot = meson.project_source_root() / '3rd-party'

sdlDir = archLibRoot / 'SDL2' / 'lib' / libraryPlatform / libraryConfiguration
sdlImageDir = archLibRoot / 'SDL2_image' / 'lib' / libraryPlatform / libraryConfiguration
sdlMixerDir = archLibRoot / 'SDL_mixer' / 'lib' / libraryPlatform / libraryConfiguration
crashRptDir = archLibRoot / 'CrashRpt' / 'lib' / libraryPlatform / libraryConfiguration
zlibDir = archLibRoot / 'zlib-ng' / 'lib' / libraryPlatform
minizipDir = archLibRoot / 'minizip' / 'lib' / libraryPlatform / libraryConfiguration

sdlName = isDebugBuild ? 'SDL2d' : 'SDL2'
sdlMainName = isDebugBuild ? 'SDL2maind' : 'SDL2main'
crashRptName = isDebugBuild ? 'CrashRpt1403d' : 'CrashRpt1403'
crashProbeName = isDebugBuild ? 'CrashRptProbe1403d' : 'CrashRptProbe1403'

commonLibraries = [
    cppCompiler.find_library('zlib', dirs: zlibDir),
    cppCompiler.find_library('libminizip', dirs: minizipDir),
    cppCompiler.find_library(sdlName, dirs: sdlDir),
    cppCompiler.find_library(sdlMainName, dirs: sdlDir),
    cppCompiler.find_library('SDL2_image', dirs: sdlImageDir),
    cppCompiler.find_library('winmm'),
]
projectLibraries = commonLibraries + [
    cppCompiler.find_library('SDL2_mixer', dirs: sdlMixerDir),
    cppCompiler.find_library(crashRptName, dirs: crashRptDir),
    cppCompiler.find_library(crashProbeName, dirs: crashRptDir),
]

subdir('ida2asm')
subdir('swos')
subdir('mnu2h')

# Keep asset compilation incremental while retaining the same tmp/assets output
# tree used by the Visual Studio projects and by the running game.
c = run_command([python, '-c', getFilesCommand.format('assets', '*.png')], check: true)
assetInputs = c.stdout().strip().split('\n')
c = run_command([python, '-c', getFilesCommand.format('assets', '*.txt')], check: true)
assetInputs += c.stdout().strip().split('\n')
c = run_command([python, '-c', getFilesCommand.format('3rd-party/PyTexturePacker', '*.py')], check: true)
assetInputs += c.stdout().strip().split('\n')

assetHeaders = custom_target('assets',
    input: assetInputs,
    output: ['spriteDatabase.h', 'variableSprites.h', 'pitchDatabase.h', 'stadiumSprites.h', 'assets.stamp'],
    depend_files: files('assets' / 'compileAssets.py', 'assets' / 'compileAssetsForMeson.py',
        'assets' / 'requirements.txt'),
    command: [python, files('assets' / 'compileAssetsForMeson.py'), '@OUTDIR@'],
    build_by_default: true,
)

subdir('src')

executable('swos-port',
    sourceFiles,
    swosGeneratedSources,
    swosCpp[0],
    menuFiles,
    animationTables[0],
    assetHeaders,
    include_directories: [thirdPartyIncludeDirs, srcIncludeDirs],
    dependencies: projectLibraries,
    cpp_args: commonCppArgs,
    cpp_pch: 'src' / 'stdinc.h',
)

subdir('tests')
