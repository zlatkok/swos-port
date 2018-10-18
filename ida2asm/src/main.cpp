// Simplified parser for transforming IDA Pro asm output into one or more files that can actually be assembled.
// Experimenting with using data-flow based programming paradigm. With the previous parser being a flop
// (~2 minutes to process the SWOS asm file), this one being blazing fast is simply a must!

#include "Util.h"
#include "AsmConverter.h"

struct CommandLineParameters {
    const char *inputPath;
    const char *outputPath;
    const char *symbolFilePath;
    const char *swosHeaderPath;
    const char *format;
    int numOutputFiles;
};

constexpr int kMaxOutputFiles = 20;

static CommandLineParameters getCommandLineParameters(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
        Util::exit("usage: %s <input IDA asm file path> <output asm files path> <input symbols file> <SWOS header path> "
            "<format> <number of output files>\n", EXIT_SUCCESS, Util::getFilename(argv[0]));

    if (argc < 3)
        Util::exit("Missing output assembly file path.");

    if (argc < 4)
        Util::exit("Missing input symbols file.");

    if (argc < 5)
        Util::exit("Missing SWOS header file path.");

    if (argc < 6)
        Util::exit("Missing output format.");

    if (argc < 7)
        Util::exit("Missing value for number of output assembly files.");

    int numFiles = atoi(argv[6]);
    if (numFiles > kMaxOutputFiles)
        Util::exit("Too many output files given, it should be at most %d files.", EXIT_FAILURE, kMaxOutputFiles);

    return { argv[1], argv[2], argv[3], argv[4], argv[5], numFiles, };
}

static auto start = std::chrono::high_resolution_clock::now();

int main(int argc, char **argv)
{
    atexit([]() {
        auto end = std::chrono::high_resolution_clock::now();
        int64_t timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "Done. Elapsed time: " << Util::formatNumberWithCommas(timeElapsed) << "ms.\n";
#ifndef NDEBUG
        __debugbreak();
#endif
    });

    auto params = getCommandLineParameters(argc, argv);

    AsmConverter converter(params.inputPath, params.outputPath, params.symbolFilePath, params.swosHeaderPath,
        params.format, params.numOutputFiles);
    converter.convert();

    return EXIT_SUCCESS;
}
