#pragma once

#include "file.h"
#include "util.h"
#include "MenuEntry.h"

namespace SWOS_UnitTest
{
    struct BaseException
    {
        BaseException(const char *file, int line) {
            m_error = std::string(getBasename(file)) + '(' + std::to_string(line) + "): ";
            debugBreakIfDebugged();
        }

        std::string error() const {
            return m_error;
        }

        static std::string getEntryStringWithOrdinal(const MenuEntry *entry) {
            return "menu entry #" + std::to_string(entry->ordinal);
        }

    protected:
        std::string m_error;
    };

    struct AssertionException : public BaseException
    {
        AssertionException(const char *assertExpStr, const char *file, int line) : BaseException(file, line) {
            m_error += "assertion failed: "s + assertExpStr;
        }
    };

    struct InvalidNumberOfEntriesInMenu : public BaseException
    {
        InvalidNumberOfEntriesInMenu(int actualNumItems, int expectedNumItems, const char *expectedStr, const char *file, int line)
            : BaseException(file, line) {
            m_error += "Menu has " + std::to_string(actualNumItems) + " but expecting " + std::to_string(expectedNumItems) +
                + " (" + expectedStr + ") items";
        }
    };

    struct InvalidEntryIndexException : public BaseException
    {
        InvalidEntryIndexException(int index, const char *indexStr, const char *file, int line) : BaseException(file, line) {
            m_error += "invalid menu entry index " + std::to_string(index) + " (" + indexStr + ')';
        }
    };

    struct InvalidEntryTypeException : public BaseException
    {
        InvalidEntryTypeException(const MenuEntry *entry, const char *expectedType, const char *file, int line)
            : BaseException(file, line) {
            assert(entry);
            m_error += getEntryStringWithOrdinal(entry) + " type expected: " + expectedType + ", got: " + entry->typeToString();
        }
    };

    struct InvalidEntryNumericValueException : public BaseException
    {
        InvalidEntryNumericValueException(const MenuEntry *entry, int expectedValue, const char *file, int line)
            : BaseException(file, line) {
            assert(entry && entry->type == kEntryNumber);
            m_error += getEntryStringWithOrdinal(entry) + " numeric value mismatch, got: " +
                std::to_string(entry->fg.number) + ", expected: " + std::to_string(expectedValue);
        }
    };

    struct InvalidEntryVisibilityException : public BaseException
    {
        InvalidEntryVisibilityException(const MenuEntry *entry, const char *file, int line) : BaseException(file, line) {
            assert(entry);
            m_error += "Expected " + getEntryStringWithOrdinal(entry) + " to be " + (entry->invisible ? "" : "in") + "visible";
        }
    };

    struct InvalidEntryStatusException : public BaseException
    {
        InvalidEntryStatusException(const MenuEntry *entry, const char *file, int line) : BaseException(file, line) {
            assert(entry);
            m_error += "Expected " + getEntryStringWithOrdinal(entry) + " to be " + (entry->disabled ? "en" : "dis") + "abled";
        }
    };

    struct InvalidEntryColorException : public BaseException
    {
        InvalidEntryColorException(const MenuEntry *entry, int color, const char *colorStr, const char *file, int line, bool isText = false)
            : BaseException(file, line)
        {
            assert(entry);
            auto actualColor = isText ? entry->stringFlags : entry->bg.entryColor;
            m_error += getEntryStringWithOrdinal(entry) + " has invalid " + (isText ? "text " : "") + "color " +
                std::to_string(actualColor) + ", expected " + std::to_string(color) + " (" + colorStr + ')';
        }
    };

    struct EntryStringMismatch : public BaseException
    {
        EntryStringMismatch(const MenuEntry *entry, const char *expectedString, const char *file, int line)
            : BaseException(file, line) {
            assert(entry && (entry->type == kEntryString || entry->type == kEntryStringTable));
            auto string = entry->type == kEntryString ? entry->fg.string : entry->fg.stringTable->currentString();
            m_error += getEntryStringWithOrdinal(entry) + " has unexpected string value `" +
                string.asCharPtr() + "', expected `" + expectedString + '\'';
        }
    };

    struct EntryOnSelectMissing : public BaseException
    {
        EntryOnSelectMissing(const MenuEntry *entry, const char *file, int line) : BaseException(file, line)
        {
            assert(entry && !entry->onSelect);
            m_error += getEntryStringWithOrdinal(entry) + " is missing on select function";
        }
    };

    namespace Detail {
        template<typename T>
        static inline std::string stringify(const std::vector<T>& v) {
            std::string result(1, '[');
            for (const auto& el : v)
                result += stringify(el) + ',';
            if (result.back() == ',')
                result.erase(result.end() - 1);
            return result + ']';
        }
        template<typename T>
        static inline std::string stringify(T t) {
            return std::to_string(t);
        }
        template<typename T>
        static inline std::string stringify(const std::pair<T, T>& t) {
            return "(" + std::to_string(t.first) + ", " + std::to_string(t.second) + ")";
        }
        template<typename T>
        static inline std::string stringify(T *t) {
            char buf[32];
            _ultoa(reinterpret_cast<size_t>(t), buf, 16);
            return "0x"s + buf;
        }
        static inline std::string stringify(char *str) {
            return "\""s + str + '"';
        }
        static inline std::string stringify(const char *str) {
            return "\""s + str + '"';
        }
        static inline std::string stringify(const std::string& str) {
            return '"' + str + '"';
        }
    };

    struct FailedAssertTrueException : public BaseException
    {
        FailedAssertTrueException(const char *exprStr, const char *file, int line)
            : BaseException(file, line) {
            m_error += "assert failed, expression "s + exprStr + " is false";
        }
    };

    template<typename T1, typename T2>
    struct FailedEqualAssertException : public BaseException
    {
        FailedEqualAssertException(bool mustBeEqual, const T1& t1, const T2& t2, const char *t1Str,
            const char *t2Str, const char *file, int line)
            : BaseException(file, line) {
            using namespace Detail;
            auto equalityOperator = mustBeEqual ? " != " : " == ";
            m_error += std::string(t1Str) + equalityOperator + t2Str + " (" + stringify(t1) + " != " + stringify(t2) + ')';
        }
    };


    void assertImp(bool expr, const char *exprStr, const char *file, int line);
    bool ignoreAsserts(bool ignored);

    class AssertSilencer {
        bool m_oldState;
    public:
        AssertSilencer() { m_oldState = ignoreAsserts(true); }
        ~AssertSilencer() { ignoreAsserts(m_oldState); }
    };
}
