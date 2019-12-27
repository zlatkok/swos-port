#include "Segment.h"
#include "Util.h"

Segment::Segment(CToken *begin, CToken *end) : m_numParams(0)
{
    assert(begin && end && end > begin);
    assert(begin->next() && (begin->next()->type == Token::T_SEGMENT || begin->next()->type == Token::T_ENDS));

    auto param = this->begin();

    while (begin != end) {
        new ((void *)param) Param(begin);
        param = param->next();
        advance(begin);
        m_numParams++;
    }
}

size_t Segment::requiredSize(CToken *begin, CToken *end)
{
    assert(begin && end && end > begin);
    assert(begin->next() && (begin->next()->type == Token::T_SEGMENT || begin->next()->type == Token::T_ENDS));

    auto size = sizeof(Segment);

    while (begin != end) {
        size += Param::requiredSize(begin);
        advance(begin);
    }

    return size;
}

size_t Segment::count() const
{
    return m_numParams;
}

auto Segment::begin() const -> const Param *
{
    return (Param *)(this + 1);
}

Segment::Param::Param(CToken *param)
{
    assert(param && param->textLength);

    Util::assignSize(m_length, param->textLength);
    param->copyText(textPtr());
}

size_t Segment::Param::requiredSize(CToken *param)
{
    assert(param && param->textLength);
    return sizeof(Param) + param->textLength;
}

String Segment::Param::text() const
{
    return { textPtr(), m_length };
}

auto Segment::Param::next() const -> const Param *
{
    return reinterpret_cast<Param *>(textPtr() + m_length);
}

char *Segment::Param::textPtr() const
{
    return (char *)(this + 1);
}

void SegmentSet::add(const TokenRange& range)
{
    auto it = findSegment(range.first);

    if (it == m_segments.end())
        m_segments.emplace_back(range.first, range.second);
}

void SegmentSet::remove(CToken *segment)
{
    auto it = findSegment(segment);

    if (it != m_segments.end())
        m_segments.erase(it);
}

void SegmentSet::clear()
{
    m_segments.clear();
}

bool SegmentSet::empty() const
{
    return m_segments.empty();
}

TokenRange SegmentSet::segmentRange(CToken *seg) const
{
    auto it = findSegment(seg);
    assert(it != m_segments.end());

    return it != m_segments.end() ? *it : TokenRange();
}

bool SegmentSet::isSegment(CToken *token) const
{
    return findSegment(token) != m_segments.end();
}

bool SegmentSet::isSegment(const String& str) const
{
    auto it = std::find_if(m_segments.begin(), m_segments.end(), [&str](const auto& segRange) {
        return segRange.first->textLength == str.length() && !memcmp(segRange.first->text(), str.data(), str.length());
    });

    return it != m_segments.end();
}

auto SegmentSet::begin() const -> SegmentList::const_iterator
{
    return m_segments.cbegin();
}

auto SegmentSet::end() const -> SegmentList::const_iterator
{
    return m_segments.cend();
}

auto SegmentSet::findSegment(CToken *segment) const -> SegmentList::const_iterator
{
    auto it = std::find_if(m_segments.begin(), m_segments.end(), [segment](const auto& segmentPair) {
        return segmentPair.first == segment;
    });

    return it;
}
