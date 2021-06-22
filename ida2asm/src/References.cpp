#include "References.h"
#include "Struct.h"
#include "OutputItem/Segment.h"

constexpr int kLabelCapacity = 750'000;
constexpr int kReferencesCapacity = 2'500'000;

References::References() : m_labels(kLabelCapacity), m_references(kReferencesCapacity)
{
}

void References::addVariable(CToken *var, int size, CToken *structName)
{
    assert(size || structName);

    if (!structName && addAmigaRegister(var))
        return;

    ReferenceType type = kNone;
    switch (size) {
    case 0: type = kUser; break;
    case 1: type = kByte; break;
    case 2: type = kWord; break;
    case 4: type = kDword; break;
    case 8: type = kQWord; break;
    case 10: type = kTbyte; break;
    default: assert(false); break;
    }

    m_labels.add(var, type, structName);
}

void References::addProc(CToken *proc)
{
    m_labels.add(proc, kProc);
}

void References::addLabel(CToken *label)
{
    assert(label && label->isId() && label->textLength);

    int endColon = label->lastChar() == ':';
    m_labels.add(label->text(), label->textLength - endColon, kNear);
}

void References::addReference(CToken *ref)
{
    if (isReference(ref))
        m_references.add(ref);
}

void References::addReference(const String& str)
{
    if (isReference(str))
        m_references.add(str.data(), str.length());
}

void References::markImport(const String& str)
{
    if (auto ref = m_references.get(str))
        ref->type = kNear;
}

void References::markExport(const String& str)
{
    if (auto label = m_labels.get(str))
        label->pub = 1;
}

bool References::hasReference(const String& str) const
{
    auto ref = m_references.get(str);
    return ref && ref->type != kIgnore && ref->type != kNone;
}

bool References::hasPublic(const String& str) const
{
    auto label = m_labels.get(str);
    return label && label->pub;
}

auto References::getType(const String& str) const -> std::pair<ReferenceType, String>
{
    assert(!str.empty());

    auto ref = m_references.get(str);

    auto type = kUnknown;
    String userType;

    if (ref) {
        type = ref->type;
        userType = ref->structName();
    }

    return { type, userType };
}

void References::setIgnored(const String& str, Util::hash_t hash)
{
    for (auto ref : m_references.getAll(str, hash))
        ref->type = kIgnore;
}

void References::clear()
{
    m_labels.clear();
    m_references.clear();
    m_amigaRegisters.fill(false);
}

void References::seal()
{
    m_labels.seal();

    assert(!m_labels.hasDuplicates());

    m_references.seal();
    m_references.removeDuplicates();
}

void References::resolve()
{
    resolve(*this);
}

void References::resolve(const References& rhs)
{
    for (const auto& ref : m_references) {
        auto& type = ref.cargo->type;
        if (type == kNone) {
            auto label = rhs.m_labels.get(ref.text, ref.hash);
            if (label) {
                if (&rhs == this) {
                    type = kIgnore;
                } else {
                    assert(label->type != kNone);
                    type = label->type;

                    if (type == kUser) {
                        assert(!label->structPtr);
                        ref.cargo->structPtr = label->structNameLength();
                    }

                    label->pub = 1;
                }
            }
        }
    }
}

void References::resolveSegments(const SegmentSet& segments)
{
    for (const auto& ref : m_references)
        if (segments.isSegment(ref.text))
            ref.cargo->type = kIgnore;
}

std::vector<String> References::publics() const
{
    decltype(publics()) result;
    result.reserve(m_labels.count() + kNumAmigaRegisters);

    for (size_t i = 0; i < m_amigaRegisters.size(); i++)
        if (m_amigaRegisters[i])
            result.emplace_back(indexToAmigaRegister(i));

    for (const auto& ref : m_labels)
        if (ref.cargo->pub)
            result.emplace_back(ref.text);

    return result;
}

auto References::externs() const -> std::vector<std::tuple<String, ReferenceType, String>>
{
    decltype(externs()) result;
    result.reserve(m_references.count() + kNumAmigaRegisters);

    // make things simple, always import Amiga registers, except if they're defined here
    for (int i = 0; i < kNumAmigaRegisters; i++)
        if (!m_amigaRegisters[i])
            result.emplace_back(std::make_tuple(indexToAmigaRegister(i), kDword, String()));

    for (auto& ext : m_references)
        if (ext.cargo->type != kIgnore)
            result.emplace_back(std::make_tuple(ext.text, ext.cargo->type, ext.cargo->structName()));

    return result;
}

bool References::isReference(const String& str)
{
    return str != '$' && str != ')' && !str.contains(':') && str != "st" &&
        str != "offset" && amigaRegisterToIndex(str) < 0;
}

bool References::addAmigaRegister(CToken *reference)
{
    int amigaReg = amigaRegisterToIndex(reference);
    if (amigaReg >= 0) {
        assert(amigaReg >= 0 && amigaReg < static_cast<int>(m_amigaRegisters.size()));
        m_amigaRegisters[amigaReg] = true;
        return true;
    }

    return false;
}
