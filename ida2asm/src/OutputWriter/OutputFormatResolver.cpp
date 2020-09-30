#include "OutputFormatResolver.h"

OutputFormatResolver::OutputFormat OutputFormatResolver::resolveOutputFormat(const char *format)
{
    for (const auto& formatPair : kFormats)
        if (!_stricmp(formatPair.name, format))
            return formatPair.type;

    return kUnknown;
}

std::string OutputFormatResolver::getSupportedFormats()
{
    std::string formats;
    for (auto format : kFormats)
        formats += ", "s + format.name;

    return formats.substr(2);
}
