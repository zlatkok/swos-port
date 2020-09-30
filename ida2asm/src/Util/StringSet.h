#pragma once

#include "DynaArray.h"
#include "Tokenizer.h"
#include "Iterator.h"

class StringSet
{
public:
#pragma pack(push, 1)
    class Node
    {
    public:
        Node(const char *str, size_t len, Util::hash_t hash) : m_hash(hash) {
            Util::assignSize(m_len, len);
            memcpy(text(), str, len);
        }
        static size_t requiredSize(size_t len) { return sizeof(Node) + len; }

        void remove() { m_removed = 1; }
        bool removed() const { return m_removed != 0; }

        String string() const { return { text(), textLength() }; };
        char *text() const { return (char *)(this + 1); }
        size_t textLength() const { return m_len; }
        Util::hash_t hash() const { return m_hash; }

        Node *next() const { return reinterpret_cast<Node *>(text() + m_len); }
    private:
        uint8_t m_removed = 0;
        uint8_t m_len;
        Util::hash_t m_hash;
    };
#pragma pack(pop)

    StringSet(size_t initialCapacity);
    void add(CToken *token);
    void add(const String& str);
    void add(const char *str, size_t len);
    void add(const char *begin, const char *end);
    bool empty() const;
    bool present(const String& str) const;
    bool present(const char *str, size_t len) const;
    void seal();
    Iterator::Iterator<Node> begin() const;
    Iterator::Iterator<Node> end() const;

private:
    void add(const char *str, size_t len, Util::hash_t hash);

    struct LookupNode {
        Node *node;
        Util::hash_t hash;
        size_t len;
        bool operator<(const LookupNode& rhs) const {
            return hash < rhs.hash;
        }
        bool operator==(const LookupNode& rhs) const {
            return hash == rhs.hash && len == rhs.len && !memcmp(node->text(), rhs.node->text(), len);
        }
    };

    struct NodeComp {
        bool operator()(const LookupNode& node, Util::hash_t hash) const {
            return node.hash < hash;
        }
        bool operator()(Util::hash_t hash, const LookupNode& node) const {
            return hash < node.hash;
        }
    };

private:
    DynaArray m_data;
    size_t m_count = 0;
    LookupNode *m_nodes = nullptr;
};
