#include "ProcHookList.h"

void ProcHookList::add(const String& procName, const String& hookName, int line, int definedAtLine)
{
    ProcHookItemInternal item;
    auto procLen = std::min(procName.length(), kProcNameLength - 1);
    memcpy(item.procName, procName.data(), procLen);
    item.procName[procLen] = '\0';
    item.procNameLen = procLen;

    auto hookLen = std::min(hookName.length(), kProcNameLength - 1);
    memcpy(item.hookName, hookName.data(), hookLen);
    item.hookName[hookLen] = '\0';
    item.hookNameLen = hookLen;

    item.line = line;
    item.definedAtLine = definedAtLine;

    m_items.push_back(item);
}

auto ProcHookList::getItems() -> std::vector<ProcHookItem>
{
    if (m_sortedItems.size() != m_items.size())
        m_sortedItems = getSortedItems();

    std::vector<ProcHookItem> result;
    for (size_t i = 0; i < m_sortedItems.size(); i++) {
        const auto& item = m_sortedItems[i];
        int nextIndex = item->lastIndex > 0 ? item->lastIndex : i + 1;

        result.emplace_back(String(item->procName, item->procNameLen), String(item->hookName, item->hookNameLen),
            item->line, nextIndex, item->definedAtLine);
    }

    return result;
}

String ProcHookList::encodeProcHook(const ProcHookItem *begin, const ProcHookItem *end)
{
    auto requiredSize = PackedProcHookList::requiredSize(begin, end);

    auto buf = new char[requiredSize];
    new (buf) PackedProcHookList(begin, end, requiredSize);

    return { buf, requiredSize };
}

int ProcHookList::getCurrentHookLine(const String& procHook)
{
    auto hookList = (PackedProcHookList *)procHook.data();
    return hookList->getCurrentLine();
}

String ProcHookList::getCurrentHookProc(const String& procHook)
{
    auto hookList = (PackedProcHookList *)procHook.data();
    return hookList->getCurrentHookProc();
}

bool ProcHookList::moveToNextHook(const String& procHook)
{
    auto hookList = (PackedProcHookList *)procHook.data();
    return hookList->moveToNextHook();
}

// Sorts all items, by proc first and then by line number. Also set lastItem field which is used to group
// items belonging to the same proc.
auto ProcHookList::getSortedItems() -> std::vector<ProcHookItemInternal *>
{
    std::vector<ProcHookItemInternal *> sortedItems;
    sortedItems.reserve(m_items.size());

    for (auto& item : m_items)
        sortedItems.push_back(&item);

    // sort using insertion sort, maintaining last index among hooks in the same proc
    for (size_t i = 1; i < m_items.size(); i++) {
        auto current = sortedItems[i];

        int j = i - 1;
        for (; j >= 0; j--) {
            int cmp = strcmp(current->procName, sortedItems[j]->procName);
            int lastIndex = j + 2;

            if (cmp > 0) {
                // this must be the first hook of this proc
                if (current->lastIndex < 0)
                    current->lastIndex = lastIndex;
                current->first = true;
                break;
            } else if (cmp == 0) {
                // there is at least one other hook
                if (current->line > sortedItems[j]->line) {
                    assert(j > 0);
                    if (current->lastIndex < 0)
                        current->lastIndex = lastIndex;

                    // update the first hook for this proc
                    int first = j;
                    while (!sortedItems[first]->first)
                        first--;

                    sortedItems[first]->lastIndex = current->lastIndex;
                    break;
                }

                // since we're lower, this item can't be first
                sortedItems[j]->first = false;

                if (current->lastIndex < 0)
                    current->lastIndex = lastIndex;
            }

            // must update lastIndex of each item we bubble up (if valid)
            if (sortedItems[j]->lastIndex >= 0)
                sortedItems[j]->lastIndex++;

            sortedItems[j + 1] = sortedItems[j];
        }

        sortedItems[j + 1] = current;
        if (j <= 0)
            sortedItems[0]->first = true;
    }

    return sortedItems;
}

ProcHookList::PackedProcHookList::PackedProcHookList(const ProcHookItem *begin, const ProcHookItem *end, size_t size)
{
    assert(end > begin);

    m_sentinel = (char *)this + size;
    m_currentHook = reinterpret_cast<HookHeader *>((char *)(this + 1) + begin->procName.length());

    Util::assignSize(m_procNameLen, begin->procName.length());
    begin->procName.copy((char *)(this + 1));

    auto header = reinterpret_cast<HookHeader *>(m_currentHook);

    for (auto it = begin; it != end; it++) {
        assert(it->procName == begin->procName);
        assert(it == begin || (it->line >= it[-1].line && (it->line != it[-1].line || it->definedAtLine < it[-1].definedAtLine)));

        header->line = it->line;
        header->hookNameLen = it->hookName.length();
        it->hookName.copy((char *)(header + 1));

        header = header->next();
    }
}

size_t ProcHookList::PackedProcHookList::requiredSize(const ProcHookItem *begin, const ProcHookItem *end)
{
    assert(end > begin);

    size_t requiredSize = 0;

    for (auto it = begin; it != end; it++) {
        assert(it->procName == begin->procName);

        requiredSize += it->hookName.length() + sizeof(HookHeader);
    }

    return sizeof(PackedProcHookList) + requiredSize + begin->procName.length();
}

bool ProcHookList::PackedProcHookList::moveToNextHook()
{
    if (m_currentHookCharPtr >= m_sentinel)
        return false;

    m_currentHook = m_currentHook->next();
    assert(m_currentHookCharPtr <= m_sentinel);

    return m_currentHookCharPtr < m_sentinel;
}

int ProcHookList::PackedProcHookList::getCurrentLine() const
{
    assert(m_currentHookCharPtr < m_sentinel);
    return m_currentHook->line;
}

String ProcHookList::PackedProcHookList::getCurrentHookProc() const
{
    assert(m_currentHookCharPtr < m_sentinel);
    return m_currentHook->hookName();
}
