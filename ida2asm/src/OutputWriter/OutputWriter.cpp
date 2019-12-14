#include "OutputWriter.h"

OutputWriter::OutputWriter(const char *path, const SymbolFileParser& symFileParser, const StructStream& structs, const DefinesMap& defines,
    const References& references, const OutputItemStream& outputItems)
:
    m_symFileParser(symFileParser), m_structs(structs), m_defines(defines),
    m_references(references), m_outputItems(outputItems), m_path(path)
{
}

OutputWriter::~OutputWriter()
{
    closeOutputFile();
}

bool OutputWriter::openOutputFile(OutputFlags flags, int anticipatedSize /* = -1 */)
{
    const auto& path = getOutputFilename(flags);

    if (anticipatedSize < 0) {
        // this is probably an overstatement, but the real size should surely be less (I hope!)
        anticipatedSize = m_structs.size() + m_defines.size() + m_outputItems.size();
    }

    return openOutputFile(path.c_str(), anticipatedSize);
}

std::string OutputWriter::getOutputFilename(OutputFlags flags)
{
    bool isDefinesFile = (flags & (kStructs | kDefines)) && !(flags & kFullDisasembly);

    if (isDefinesFile) {
        auto outputBasePath = getOutputBaseDir();
        return Util::joinPaths(outputBasePath, getDefsFilename());
    }

    return m_path;
}

std::string OutputWriter::getOutputBaseDir()
{
    return Util::getBasePath(m_path);
}

void OutputWriter::closeOutputFile()
{
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

int OutputWriter::outputTab(int column)
{
    static_assert(sizeof(kIndent) == kTabSize + 1, "Indent not equal to tab size");

    int nextTab = (column + kTabSize) / kTabSize * kTabSize;
    out(kIndent + kTabSize - nextTab + column);

    return column;
}

// take special care not to contaminate the file with tabs
int OutputWriter::outputComment(const String& comment, int column /* = 0 */)
{
    bool isLeading = column == 0;

    for (size_t i = 0; i < comment.length(); i++) {
        auto c = comment.str()[i];
        if (c == '\t') {
            column = outputTab(column);
        } else {
            if (c == '\n')
                column = 0;
            else
                column++;
            out(c);
        }
    }

    if (!isLeading)
        out(Util::kNewLine);

    return column;
}

bool OutputWriter::save()
{
    if (m_file) {
        auto size = m_outPtr - m_outBuffer.get();
        return fwrite(m_outBuffer.get(), size, 1, m_file) == 1;
    }

    if (!m_error.empty())
        m_error += '\n';
    m_error += "write failed";

    assert(false);
    return false;
}

size_t OutputWriter::outputLength() const
{
    return m_outPtr > m_outBuffer.get() ? m_outPtr - m_outBuffer.get() : 0;
}

char *OutputWriter::getOutputPtr() const
{
    return m_outPtr;
}

void OutputWriter::setOutputPtr(char *ptr)
{
    m_outPtr = ptr;
}

void OutputWriter::unget(size_t numChars)
{
    m_outPtr -= numChars;
    assert(m_outPtr >= m_outBuffer.get());
}

bool OutputWriter::openOutputFile(const char *path, size_t requiredSize)
{
    if (m_file = fopen(path, "wb")) {
        std::setbuf(m_file, nullptr);

        m_outBufferSize = requiredSize;
        m_outBuffer.reset(new char[m_outBufferSize]);

        m_outPtr = m_outBuffer.get();

        return true;
    }

    m_error = "output file not open";
    return false;
}
