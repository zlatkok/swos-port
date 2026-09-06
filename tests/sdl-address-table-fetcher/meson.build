fetcherBaseName = 'SdlAddressTableFetcher'
fetcherDllName = fetcherBaseName + '.dll'
fetcherDefine = '-DSDL_ADDRESS_FETCHER_DLL="@0@"'.format(fetcherDllName)

sdlAddressTableFetcher = shared_library(fetcherBaseName,
    'src' / 'SdlAddressTableFetcher.cpp',
    include_directories: thirdPartyIncludeDirs,
    cpp_args: ['-D_WINDLL', '-DSDL_DYNAPI_FETCHER_EXPORTS', fetcherDefine],
)
