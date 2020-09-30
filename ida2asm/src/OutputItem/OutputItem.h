#pragma once

#include "DynaArray.h"
#include "Tokenizer.h"
#include "Instruction.h"
#include "Label.h"
#include "Proc.h"
#include "EndProc.h"
#include "Segment.h"
#include "Directive.h"
#include "StackVariable.h"
#include "Iterator.h"

class DataItem;

#pragma pack(push, 1)
class OutputItem
{
public:
    enum Type : uint8_t {
        kInstruction,
        kDataItem,
        kProc,
        kEndProc,
        kLabel,
        kStackVariable,
        kDirective,
        kSegment,
        kComment,
    };

    OutputItem(Type type, size_t size, const TokenList& leadingComments, CToken *comment);
    OutputItem(const OutputItem&) = delete;
    static size_t requiredSize(const TokenList& leadingComments, CToken *comment);
    Type type() const;
    String leadingComments() const;
    String comment() const;
    size_t size() const;
    void increaseSize(size_t size);
    OutputItem *next() const;
    template<typename T>
    T *getItem() const
    {
        assert(m_size > 0);
        return reinterpret_cast<T *>(tail());
    }

private:
    char *leadingCommentsPtr() const;
    char *commentPtr() const;
    void *tail() const;

    uint16_t m_leadingCommentsLength;
    uint16_t m_size;
    Type m_type;
    uint8_t m_lineCommentLength;
};
#pragma pack(pop)

class OutputItemStream
{
public:
    OutputItemStream();
    size_t size() const;
    void addInstruction(const TokenList& leadingComments, CToken *comment, CToken *prefix,
        CToken *insnToken, const Instruction::OperandSizes& opSizes, const Instruction::OperandTypes& opTypes,
        const Instruction::OperandTokens& opTokens);
    void addProc(const TokenList& leadingComments, CToken *comment, CToken *name);
    void addEndProc(const TokenList& leadingComments, CToken *comment, CToken *name);
    void addStackVariable(const TokenList& leadingComments, CToken *comment, const String& name, CToken *size, CToken *offset);
    void addDataItem(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *structName, uint8_t baseSize);
    void addDataElement(CToken *token, bool isOffset, int offset, size_t dup);
    void addLabel(const TokenList& leadingComments, CToken *comment, CToken *token);
    void addDirective(const TokenList& leadingComments, CToken *comment, CToken *begin, CToken *end);
    void addSegmentStartOrEnd(const TokenList& leadingComments, CToken *comment, CToken *begin, CToken *end);
    void addTrailingComments(const TokenList& comments);
    OutputItem *lastItem() const;
    DataItem *lastDataItem() const;
    const Iterator::Iterator<const OutputItem> begin() const;
    const Iterator::Iterator<const OutputItem> end() const;
    void clear();

private:
    template<typename T, OutputItem::Type type, typename... Args>
    void addOutputItem(const TokenList& leadingComments, CToken *comment, size_t itemSize, Args... args);

    DynaArray m_items;

    OutputItem *m_lastItem = nullptr;
    Proc *m_currentProc = nullptr;
};
