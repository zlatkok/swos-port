#pragma once

#include "DynaArray.h"
#include "Util.h"
#include "Tokenizer.h"
#include "StringView.h"
#include "Iterator.h"

template<class T>
class StringMap
{
public:
    class Iterator
    {
    public:
        struct Node {
            String text;
            Util::hash_t hash;
            T *cargo;
        };
        Iterator(typename StringMap::Node *node) {
            assignNode(node);
        }
        Iterator& operator++() {
            assignNode(m_node->next());
            return *this;
        }
        const Node *operator*() const {
            return &result;
        }
        const Node *operator->() const {
            return &result;
        }
        bool operator!=(const Iterator& rhs) const {
            return m_node != rhs.m_node;
        }
        String text() const { return m_node->text(); }
        operator String() const { return m_node->text(); }
    private:
        void assignNode(typename StringMap::Node *node) {
            m_node = node;
            result.text = node->text();
            result.hash = node->hash();
            result.cargo = node->cargo();
        }
        typename StringMap::Node *m_node;
        Node result;
    };

    StringMap(size_t initialCapacity) : m_data(initialCapacity) {}
    size_t size() const { return m_data.spaceUsed(); }
    size_t count() const { return m_count; }
    Iterator begin() const {
        auto node = reinterpret_cast<Node *>(m_data.begin());

        while (node->removed())
            node = node->next();

        return node;
    }
    Iterator end() const {
        assert(!m_count || m_end);
        return Iterator(m_end ? m_end : reinterpret_cast<Node *>(m_data.end()));
    }

    template<typename... Args>
    void add(const String& str, Args... args)
    {
        add(str.str(), str.length(), Util::hash(str, len), args...);
    }

    template<typename... Args>
    void add(const char *str, size_t len, Args... args) {
        add(str, len, Util::hash(str, len), args...);
    }

    template<typename... Args>
    void add(CToken *token, Args... args)
    {
        assert(token && token->isId() && token->textLength);
        add(token->text(), token->textLength, token->hash, args...);
    }

    template<typename... Args>
    void add(const char *str, size_t len, Util::hash_t hash, Args... args) {
        assert(!m_nodes);

        auto size = T::requiredSize(args...);
        auto nodeSize = Node::requiredSize(len);

        auto buf = m_data.add(size + nodeSize);
        new (buf) Node(str, len, hash, size, args...);

        m_count++;
    }

    T *get(const char *str, size_t len) const {
        return get(str, len, Util::hash(str, len));
    }

    T *get(const String& str) const {
        return get(str, Util::hash(str.str(), str.length()));
    }

    T *get(const String& str, Util::hash_t hash) const {
        return get(str.str(), str.length(), hash);
    }

    T *get(CToken *token) const {
        assert(token->isId() && token->textLength);
        return get(token->text(), token->textLength, token->hash);
    }

    T *get(const char *str, size_t len, Util::hash_t hash) const {
        auto range = getRange(str, len, hash);

        for (auto it = range.first; it != range.second; ++it)
            if (it->equal(str, len))
                return it->node->cargo();

        return nullptr;
    }

    std::vector<T *> getAll(const char *str, size_t len, Util::hash_t hash) const {
        std::vector<T *> result;

        auto range = getRange(str, len, hash);
        for (auto it = range.first; it != range.second; ++it)
            if (it->equal(str, len))
                result.push_back(it->node->cargo());

        return result;
    }

    std::vector<T *> getAll(const String& str, Util::hash_t hash) const {
        return getAll(str.str(), str.length(), hash);
    }

    std::vector<T *> getAll(const String& str) const {
        return getAll(str.str(), str.length(), Util::hash(str.str(), str.length()));
    }

    std::vector<T *> getAll(CToken *token) const {
        assert(token->isId() && token->textLength);
        return getAll(token->text(), token->textLength(), token->hash);
    }

    void seal() {
        assert(!m_nodes);

        if (!m_count)
            return;

        // gotta have one non-removed node at the end, so next() doesn't loop away into outer space
        auto sentinelBuf = m_data.add(sizeof(Node));
        m_end = new (sentinelBuf) Node("z", 1, 0);

        auto alignment = (4 - m_data.spaceUsed() % 4) % 4;
        auto buf = m_data.add((m_count + 1) * sizeof(LookupNode) + alignment);
        m_nodes = reinterpret_cast<LookupNode *>(buf + alignment);

        auto node = reinterpret_cast<Node *>(m_data.begin());

        for (size_t i = 0; i < m_count; i++) {
            m_nodes[i].node = node;
            m_nodes[i].len = node->textLength();
            m_nodes[i].hash = node->hash();

            node = node->next();
        }

        std::sort(m_nodes, m_nodes + m_count);

        // set up a sentinel for sorted nodes as well, to simplify duplicate elimination
        m_nodes[m_count].hash = m_nodes[m_count - 1].hash + 1;
    }

    bool hasDuplicates() const {
        assert(m_nodes || !m_count);

        for (size_t i = 1; i < m_count; i++) {
            if (m_nodes[i].node->removed())
                return true;

            for (size_t j = i + 1; m_nodes[j].hash == m_nodes[i].hash; j++)
                if (*m_nodes[j].node == *m_nodes[i].node)
                    return true;
        }

        return false;
    }

    void removeDuplicates() {
        assert(m_nodes || !m_count);

        for (size_t i = 1; i < m_count; i++)
            if (!m_nodes[i].node->removed())
                for (size_t j = i + 1; m_nodes[j].hash == m_nodes[i].hash; j++)
                    if (*m_nodes[j].node == *m_nodes[i].node)
                        m_nodes[j].node->remove();
    }

private:
    using pointer = T *;

#pragma pack(push, 1)
    class Node
    {
    public:
        template<typename... Args>
        Node(const char *str, size_t len, Util::hash_t hash, size_t payloadLength, Args... args) : Node(str, len, hash) {
            m_hash = hash;
            Util::assignSize(m_payloadLength, payloadLength);
            new (cargo()) T(args...);
        }
        Node(const char *str, size_t len, Util::hash_t hash) : m_hash(hash), m_payloadLength(0) {
            Util::assignSize(m_textLength, len);
            memcpy(textPtr(), str, len);
        }
        static size_t requiredSize(size_t len) {
            return sizeof(Node) + len;
        }
        bool operator==(const Node& rhs) const {
            assert(m_hash == rhs.m_hash);
            return m_textLength == rhs.m_textLength && !memcmp(textPtr(), rhs.textPtr(), m_textLength);
        }
        operator String() const { return { textPtr(), textLength() }; }

        char *textPtr() const { return (char *)(this + 1); }
        size_t textLength() const { return m_textLength; }
        String text() const { return { textPtr(), m_textLength }; }
        Util::hash_t hash() const { return m_hash; }
        T *cargo() const { return reinterpret_cast<T *>(textPtr() + m_textLength); }
        bool removed() const { return *textPtr() == '\0'; }
        void remove() { *textPtr() = '\0'; }
        Node *next() const {
            auto nextNode = getNext();

            while (nextNode->removed())
                nextNode = nextNode->getNext();

            return nextNode;
        }

    private:
        Node *getNext() const { return reinterpret_cast<Node *>(reinterpret_cast<char *>(cargo()) + m_payloadLength); }

        Util::hash_t m_hash;
        uint8_t m_textLength;
        uint8_t m_payloadLength;
    };
#pragma pack(pop)

    struct LookupNode {
        Node *node;
        size_t len;
        Util::hash_t hash;

        bool operator<(const LookupNode& rhs) const {
            return hash < rhs.hash;
        }
        bool operator==(const LookupNode& rhs) const {
            assert(hash == rhs.hash);
            return len == rhs.len && !memcmp(node->textPtr(), rhs.node->textPtr(), len);
        }
        bool equal(const char *str, size_t len) const {
            return len == this->len && !memcmp(this->node->textPtr(), str, len);
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

    DynaArray m_data;
    size_t m_count = 0;
    LookupNode *m_nodes = nullptr;
    Node *m_end = nullptr;

    std::pair<LookupNode *, LookupNode *> getRange(const char *str, size_t len, Util::hash_t hash) const {
        if (!m_count)
            return {};

        assert(m_nodes);

        // force entire table in the cache
        volatile auto c = m_data.begin();
        *c;

        return std::equal_range(m_nodes, m_nodes + m_count, hash, NodeComp());
    }
};
