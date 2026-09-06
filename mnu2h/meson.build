# find mnu2h and make it globally available
mnu2h = files('mnu2h.py')

# gather mnu2h dependencies
c = run_command([python, '-c', getFilesCommand.format('.', '*.py')], check: true)
rawSources = c.stdout().strip().split('\n')

mnu2hDeps = []
foreach source : rawSources
    if not source.startswith('tests')
        mnu2hDeps += source
    endif
endforeach

mnu2hDeps = files(mnu2hDeps)
