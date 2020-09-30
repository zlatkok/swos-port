#pragma once

class OutputFormatResolver
{
public:
    enum OutputFormat {
        kVerbatim,
        kMasm,
        kCpp,
        kNumFormats,
        kUnknown,
    };

    static OutputFormat resolveOutputFormat(const char *format);
    static std::string getSupportedFormats();

private:
    constexpr struct {
        const char *name;
        OutputFormat type;
    } static const kFormats[kNumFormats] = {
        { "verbatim", kVerbatim },
        { "MASM", kMasm },
        { "C++", kCpp },
    };
};
