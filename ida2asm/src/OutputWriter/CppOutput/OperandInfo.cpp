#include "OperandInfo.h"

bool OperandInfo::operator==(const OperandInfo& other) const
{
    return type == other.type && base == other.base && scale == other.scale &&
        displacement.num == other.displacement.num && scaleFactor == other.scaleFactor && memSize == other.memSize;
}

bool OperandInfo::operator!=(const OperandInfo& other) const
{
    return !operator==(other);
}

bool OperandInfo::isConst() const
{
    assert(type != kImmediate || base.empty() && scale.empty() && !memSize && displacement.num);
    return type == kImmediate;
}

bool OperandInfo::isConstZero() const
{
    return isConst() && !*displacement.num;
}

bool OperandInfo::isFixedMem() const
{
    assert(type != kFixedMem || base.empty() && scale.empty() && memSize && displacement.num);
    return type == kFixedMem;
}

bool OperandInfo::isDynamicMem() const
{
    assert(type != kDynamicMem || (!base.empty() || !scale.empty()) && memSize);
    return type == kDynamicMem;
}

bool OperandInfo::isRegister() const
{
    assert(type != kReg || !base.empty() && scale.empty() && !memSize && (kReg < kAmigaRegsStart || !displacement.num));
    return type == kReg;
}

bool OperandInfo::needsMemoryFetch() const
{
    return isFixedMem() || isDynamicMem();
}

size_t OperandInfo::size() const
{
    switch (type) {
    case kReg:
        assert(scale.empty());
        return base.size;

    case kFixedMem:
    case kDynamicMem:
        return memSize;

    default:
        assert(false);
        // fall-through

    case kImmediate:
        return 0;
    }
}

bool OperandInfo::gotScaleFactor() const
{
    return scaleFactor != 255;
}

size_t OperandInfo::address() const
{
    assert(isFixedMem());
    return *displacement.num;
}

int OperandInfo::constValue() const
{
    assert(isConst());
    return *displacement.num;
}

void OperandInfo::setAsFixedMem(int address, uint8_t memSize)
{
    type = kFixedMem;
    base.reset();
    scale.reset();
    scaleFactor = -1;
    displacement.num = address;
    this->memSize = memSize;
}

void OperandInfo::setAsConstant(int value)
{
    type = kImmediate;
    base.reset();
    scale.reset();
    scaleFactor = -1;
    displacement.num = value;
    memSize = 0;
}

void OperandInfo::reset()
{
    type = kUnknown;
    base.reset();
    scale.reset();
    displacement.reset();

    scaleFactor = static_cast<uint8_t>(-1);
    memSize = 0;
}

size_t OperandInfo::hash() const
{
    return (type * 11) ^ base.hash() ^ scale.hash() ^ displacement.num.value_or(0x10101010) ^ scaleFactor ^ (13 * memSize);
}
