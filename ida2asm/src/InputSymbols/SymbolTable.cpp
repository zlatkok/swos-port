#include "SymbolTable.h"

constexpr int kProcCapacity = 20'000;
constexpr int kReplacementsCapacity = 600;

SymbolTable::SymbolTable() : m_procs(kProcCapacity), m_replacements(kReplacementsCapacity)
{
}

SymbolTable::SymbolTable(const SymbolTable& other)
    : m_procs(other.m_procs), m_replacements(other.m_replacements)
{
}

void SymbolTable::addSymbolAction(const char *symStart, const char *symEnd, SymbolAction action,
    const char *auxSymStart /* = nullptr */, const char *auxSymEnd /* = nullptr */)
{
    m_procs.add(symStart, symEnd - symStart, action, auxSymStart, auxSymEnd - auxSymStart);
}

void SymbolTable::addSymbolAction(const String& symStr, SymbolAction action, const String& auxSymStr)
{
    m_procs.add(symStr, action, auxSymStr);
}

void SymbolTable::addSymbolReplacement(const char *symStart, const char *symEnd, SymbolAction action, const char *replacementStart, const char *replacementEnd)
{
    assert(replacementStart && replacementEnd && replacementEnd > replacementStart);

    m_replacements.add(symStart, symEnd - symStart, action, replacementStart, replacementEnd - replacementStart);
}

void SymbolTable::seal()
{
    m_procs.seal();
    m_replacements.seal();
}

auto SymbolTable::symbolAction(CToken *sym, size_t removeMask /* = 0 */) const -> std::pair<SymbolAction, String>
{
    assert(sym && sym->isId() && sym->textLength);

    return symbolAction(String(sym), removeMask);
}

auto SymbolTable::symbolAction(const String& sym, size_t removeMask /* = 0 */) const -> std::pair<SymbolAction, String>
{
    auto holders = m_procs.getAll(sym);
    auto result = std::make_pair(kNone, String());

    for (const auto holder : holders) {
        auto action = holder->action();
        if (action != kNone) {
            result.first |= action;

            assert(result.second.empty() || holder->endSymbol().empty());

            if (!holder->endSymbol().empty())
                result.second = holder->endSymbol();

            holder->setAction(action & static_cast<SymbolAction>(removeMask));
        }
    }

    return result;
}

void SymbolTable::clearAction(const String& sym, size_t removeMask)
{
    auto holders = m_procs.getAll(sym);

    for (const auto holder : holders) {
        auto action = holder->action();
        holder->setAction(action & static_cast<SymbolAction>(removeMask));
    }
}

String SymbolTable::symbolReplacement(const String& sym) const
{
    auto holder = m_replacements.get(sym);

    if (holder) {
        assert(holder->action() == kReplace);
        return holder->endSymbol();
    }

    return {};
}

void SymbolTable::mergeSymbols(const SymbolTable &other)
{
    assert(m_procs.size() == other.m_procs.size());

    auto src = other.m_procs.begin();
    for (auto dest : m_procs) {
        assert(src != other.m_procs.end());
        assert(src->text == dest.text);

        if (src->cargo->action() == kNone)
            dest.cargo->setAction(kNone);

        ++src;
    }
}

const std::vector<String> SymbolTable::unusedSymbolsForRemoval() const
{
    std::vector<String> result;

    for (const auto& it : m_procs) {
        if (it.cargo->action() != kNone)
            result.push_back(it.text);
    }

    return result;
}
