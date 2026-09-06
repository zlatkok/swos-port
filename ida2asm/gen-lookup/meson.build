tokensInput = ['gen-lookup.py', 'tokens.lst']

# generate TokenLookup.h
tokenLookup = custom_target('token-lookup',
    input: tokensInput,
    output: ['TokenLookup.h', 'TokenTypeEnum.h'],
    depend_files: '../src/Util/Util.h',
    command: [python, '@INPUT@', '@OUTDIR@'],
)

# generates a source of TokenLookup.h implementation tester
testerMain = custom_target('generate-tester',
    input: tokensInput,
    output: 'main.cpp',
    depend_files: files('../src/Util/Util.h', '../src/Tokenizer.h'),
    # add --performance-test parameter to test performance and compare with alternative implementations
    # Meson lacks functionality to access test output, so log file needs to be examined or
    # tests must be ran with "meson test -v" instead of "ninja test"
    command: [python, '@INPUT@', '@OUTDIR@', '--test', '--performance-test'],
)

tokenLookupTesterIncludeDirs = include_directories('../src', '../src/Util')

# actually compile the lookup tester
testLookup = executable('test-lookup', testerMain, tokenLookup,
    cpp_args: '/wd5033',
    include_directories: tokenLookupTesterIncludeDirs
)
