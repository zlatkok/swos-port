# Compile the same menu inputs as the Visual Studio mnu2h utility project.
c = run_command([python, '-c', getFilesCommand.format('mnu', '*.mnu')], check: true)
rawMenus = c.stdout().strip().split('\n')

menuFiles = []
foreach menu : rawMenus
    dependencies = [mnu2hDeps]
    if menu != 'mnu/main.mnu'
        dependencies += files('mnu/common.mh')
    endif

    menuFiles += custom_target('mnu2h_@0@'.format(fs.name(menu)),
        input: menu,
        output: '@PLAINNAME@.h',
        depend_files: dependencies,
        command: [python, mnu2h, '@INPUT@', '@OUTPUT@'],
    )
endforeach
