#pragma once

class ProcHookList
{
public:
    static constexpr size_t kProcNameLength = 256;

    void add(const String& procName, const String& hookName, int line, const String& targetLabel,
        int initialValue, bool isVariable, int definedAtLine);

    struct ProcHookItem {
        ProcHookItem(const String& procName, const String& hookName, const String& targetLabel,
            int initialValue, int line, int nextIndex, int definedAtLine, bool isVariable)
        : procName(procName), hookName(hookName), targetLabel(targetLabel), initialValue(initialValue),
            line(line), nextIndex(nextIndex), definedAtLine(definedAtLine), isVariable(isVariable)
        {}
        String procName;
        String hookName;
        String targetLabel;
        int initialValue;
        int line;
        int definedAtLine;
        int nextIndex;
        bool isVariable;
    };

    std::vector<ProcHookItem> getItems();
    String encodeProcHook(const ProcHookItem *begin, const ProcHookItem *end);

    static int getCurrentHookLine(const String& procHook);
    static bool isCurrentHookVariable(const String& procHook);
    static String getCurrentHookProc(const String& procHook);
    static int getCurrentHookInitialValue(const String& procHook);
    static bool moveToNextHook(const String& procHook);
    static String takeLabelHook(const String& procHook, const String& label);
    static String firstUnconsumedLabelHook(const String& procHook);

private:
#pragma pack(push, 1)
    class PackedProcHookList {
    public:
        PackedProcHookList(const ProcHookItem *begin, const ProcHookItem *end, size_t size);
        static size_t requiredSize(const ProcHookItem *begin, const ProcHookItem *end);
        bool moveToNextHook();
        int getCurrentLine() const;
        int initialValue() const;
        bool isVariable() const;
        String getCurrentHookProc() const;
        String takeLabelHook(const String& label);
        String firstUnconsumedLabelHook() const;

    private:
        struct HookHeader {
            size_t size() const {
                return sizeof(*this) + targetLabelLen + hookNameLen;
            }
            HookHeader *next() const {
                return reinterpret_cast<HookHeader *>((char *)this + size());
            }
            String hookName() const {
                return { (char *)(this + 1) + targetLabelLen, hookNameLen };
            }
            String targetLabel() const {
                return { (char *)(this + 1), targetLabelLen };
            }

            uint32_t line;
            uint32_t targetLabelLen;
            uint32_t hookNameLen;
            int initialValue;
            bool isVariable;
            bool consumed;
        };

        char *m_sentinel;
        HookHeader *m_firstHook;
        union {
            HookHeader *m_currentHook;
            char *m_currentHookCharPtr;
        };
        uint8_t m_procNameLen;
    };
#pragma pack(pop)

    struct ProcHookItemInternal {
        char procName[kProcNameLength];
        char hookName[kProcNameLength];
        char targetLabel[kProcNameLength];
        unsigned procNameLen;
        unsigned hookNameLen;
        unsigned targetLabelLen;
        int initialValue;
        int line;
        int definedAtLine;
        int lastIndex = -1;
        bool isVariable = false;
        bool first = false;
    };

    std::vector<ProcHookItemInternal *> getSortedItems();

    std::vector<ProcHookItemInternal> m_items;
    std::vector<ProcHookItemInternal *> m_sortedItems;
};
