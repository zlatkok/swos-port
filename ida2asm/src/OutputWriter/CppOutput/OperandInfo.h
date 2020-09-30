#pragma once

#include "RegisterEnum.h"

// store an instruction operand in canonical form
struct OperandInfo
{
    struct ComponentValue
    {
        String str;
        std::optional<int> num;

        void reset() {
            str.clear();
            num.reset();
        }
    };

    struct Component
    {
        RegisterEnum reg = kNoReg;
        uint8_t offset = 0;
        uint8_t size = 0;
        ComponentValue val;

        bool operator==(const Component& other) const {
            return reg == other.reg && offset == other.offset && size == other.size;
        }
        bool empty() const {
            return reg == kNoReg;
        }
        bool isNumber() const {
            return val.num.has_value();
        }
        bool isConstZero() const {
            return val.num && *val.num == 0;
        }
        String str() const {
            return val.str;
        }
        void reset() {
            reg = kNoReg;
            offset = 0;
            size = 0;
            val.reset();
        }
        size_t hash() const {
            return (3 * reg) | ((5 * offset) << 8) | ((7 * size) << 16);
        }
    };

    enum OpTypeEnum { kUnknown, kReg, kFixedMem, kDynamicMem, kImmediate, };

    OpTypeEnum type = kUnknown;
    Component base;
    Component scale;
    ComponentValue displacement;

    uint8_t scaleFactor = static_cast<uint8_t>(-1);
    uint8_t memSize = 0;

    bool operator==(const OperandInfo& other) const;
    bool operator!=(const OperandInfo& other) const;
    bool isConst() const;
    bool isConstZero() const;
    bool isFixedMem() const;
    bool isDynamicMem() const;
    bool isRegister() const;
    bool needsMemoryFetch() const;
    size_t size() const;
    bool gotScaleFactor() const;
    size_t address() const;
    int constValue() const;
    void setAsFixedMem(int address, uint8_t memSize);
    void setAsConstant(int value);
    void reset();
    size_t hash() const;
};
