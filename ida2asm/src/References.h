#pragma once

#include "StringMap.h"
#include "StringSet.h"
#include "Tokenizer.h"
#include "AmigaRegs.h"
#include "Util.h"

class StructStream;
class SegmentSet;

class References
{
public:
    enum ReferenceType : uint8_t {
        kNone, kNear, kProc, kByte, kWord, kDword, kQWord, kTbyte, kUser, kIgnore,
    };

    References();

    void addVariable(CToken *var, int size, CToken *structName);
    void addProc(CToken *proc);
    void addLabel(CToken *label);
    void addReference(CToken *ref);
    void addReference(const String& str);
    void markImport(const String& str);
    void markExport(const String& str);
    bool hasReference(const String& str) const;
    bool hasPublic(const String& str) const;

    void setIgnored(const String& str, Util::hash_t hash);

    void seal();
    void resolve();
    void resolve(const References& rhs);
    void resolveSegments(const SegmentSet& segments);

    std::vector<String> publics() const;
    std::vector<std::tuple<String, ReferenceType, String>> externs() const;

private:
    static bool isReference(const String& str);

    // optimize for the special case, they're extremely heavily used
    bool addAmigaRegister(CToken *reference);

    std::array<bool, kNumAmigaRegisters> m_amigaRegisters = { false };

#pragma pack(push, 1)
    struct RefHolder {
        RefHolder(ReferenceType type = kNone, CToken *structName = nullptr) : type(type) {
            if (type == kUser) {
                assert(structName);
                Util::assignSize(*structNameLength(), structName->textLength);
                memcpy(structNamePtr(), structName->text(), structName->textLength);
            }
        }
        using LengthType = uint8_t;
        static size_t requiredSize(ReferenceType type = kNone, CToken *structName = nullptr) {
            return sizeof(RefHolder) + (type == kUser ? structName->textLength + sizeof(LengthType) : 0);
        }
        String structName() const {
            if (type == kUser) {
                if (structPtr)
                    return { reinterpret_cast<char *>(structPtr + 1),  *structPtr };
                else
                    return { structNamePtr(), *structNameLength() };
            }
            return String();
        }
        LengthType *structNameLength() const { return (LengthType *)(this + 1); }
        char *structNamePtr() const { return reinterpret_cast<char *>(structNameLength() + sizeof(LengthType)); }

        uint8_t *structPtr = nullptr;
        ReferenceType type;
        uint8_t pub = 0;
    };
#pragma pack(pop)

    StringMap<RefHolder> m_labels;
    StringMap<RefHolder> m_references;
};
