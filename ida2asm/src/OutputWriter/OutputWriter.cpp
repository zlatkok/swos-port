#include "OutputWriter.h"

OutputWriter::OutputWriter(const char *path) : m_path(path)
{
}

OutputWriter::~OutputWriter()
{
    closeOutputFile();
}

void OutputWriter::openOutputFile(size_t requiredSize)
{
    if (m_file = fopen(m_path, "wb")) {
        std::setbuf(m_file, nullptr);

        m_outBufferSize = requiredSize;
        m_outBuffer.reset(new char[m_outBufferSize]);

        m_outPtr = m_outBuffer.get();
    }
}

bool OutputWriter::isOutputFileOpen() const
{
    return m_file != nullptr;
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
int OutputWriter::outputComment(const String& comment, int column)
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

    assert(false);
    return false;
}
