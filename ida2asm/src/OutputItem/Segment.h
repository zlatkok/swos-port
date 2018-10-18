#pragma once

#include "Tokenizer.h"

#pragma pack(push, 1)
class Segment
{
public:
    Segment(CToken *begin, CToken *end);
    static size_t requiredSize(CToken *begin, CToken *end);

    size_t count() const;

    class Param
    {
    public:
        Param(CToken *param);
        static size_t requiredSize(CToken *param);

        String text() const;
        const Param *next() const;

    private:
        char *textPtr() const;

        uint8_t m_length;
    };

    const Param *begin() const;

private:
    uint8_t m_numParams;
};
#pragma pack(pop)

class SegmentSet
{
public:
    using SegmentList = std::vector<TokenRange>;

    void add(const TokenRange& range);
    void remove(CToken *segment);
    bool empty() const;
    TokenRange segmentRange(CToken *seg) const;
    bool isSegment(CToken *token) const;
    bool isSegment(const String& str) const;
    SegmentList::const_iterator begin() const;
    SegmentList::const_iterator end() const;

private:
    SegmentList::const_iterator findSegment(CToken *segment) const;

    SegmentList m_segments;
};
