#pragma once

#include "StringMap.h"

class StructFieldInfoHolder
{
    uint16_t m_offset;
    uint16_t m_size;
public:
    StructFieldInfoHolder(size_t offset, size_t size) {
        Util::assignSize(m_offset, offset);
        Util::assignSize(m_size, size);
    }
    static size_t requiredSize(size_t, size_t) { return sizeof(StructFieldInfoHolder); }
    size_t offset() const { return m_offset; }
    size_t size() const { return m_size; }
};

using StructMap = StringMap<StructFieldInfoHolder>;
