#pragma once

#include "DataBank.h"

class Struct;
class SymbolFileParser;

class VmFileWriter
{
public:
    static constexpr char kCppFilename[] = "vm.cpp";
    static constexpr char kHeaderFilename[] = "vm.h";

    VmFileWriter(const std::string& baseDir, int extraMemorySize, const SymbolFileParser& symFileParser,
        const DataBank& dataBank, const StructStream& structs);
    void outputVmFiles();

private:
    static constexpr int kLineLimit = 100;

    void outputCppFile();
    void outputHeaderFile();
    void outputVariablesStruct();
    static size_t getElementSize(const String& type);
    void outputVariablesEnum();
    void outputProcIndices();
    size_t outputPointerVariable(const DataBank::Var& var, size_t byteSkip);
    void outputMemoryArray();
    void outputProcExterns();
    void outputProcVector();
    void outputProcFunctions();
    void outputMemoryAccessFunctions();
    void outputDebugFunctions();
    size_t memArraySize() const;
    FILE *outputFile(const char *filename, const char *contents, size_t size);
    void outputZeroMemoryRegion();
    void outputComment(const String& comment, bool skipInitialCommentMark = false);
    String trimComment(const String& comment);
    void outputOriginalValue(const DataBank::Var& var, size_t offset);
    void outputInt(size_t val, size_t size);
    void outputString(const DataBank::Var& var);
    void outputChar(char c);
    void outputOffset(const DataBank::Var& var);
    size_t getStructFieldOffset(const Struct& struc, const String& field) const;
    void hexByteToFile(uint8_t byte);
    bool newLineNeeded() const;
    void addNewLine();
    void xfopen(const char *filename);
    void xfclose();
    void xfwrite(const void *buf, size_t size);
    void xfputs(const String& str);
    void xfputs(const char *str, bool updatePos = false);
    void xfputc(int c, bool updatePos = true);
    void xfprintf(const char *format, ...);
    void fileError();
    void updatePosition(const char *str);
    void updatePosition(char c);

    const std::string m_baseDir;
    int m_extendedMemorySize;
    const SymbolFileParser& m_symFileParser;
    const DataBank& m_dataBank;
    const StructStream& m_structs;

    FILE *m_f = nullptr;
    int m_pos = 0;
    const char *m_filename;
};
