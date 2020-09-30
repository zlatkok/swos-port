#include "StringSet.h"
#include "Util.h"

StringSet::StringSet(size_t initialCapacity) : m_data(initialCapacity)
{
}

void StringSet::add(CToken *token)
{
    assert(token->isId());
    add(token->text(), token->textLength, token->hash);
}

void StringSet::add(const String& str)
{
    add(str.data(), str.length(), Util::hash(str.data(), str.length()));
}

void StringSet::add(const char *str, size_t len)
{
    add(str, len, Util::hash(str, len));
}

void StringSet::add(const char *begin, const char *end)
{
    assert(end >= begin);
    add(begin, end - begin);
}

bool StringSet::empty() const
{
    return m_count == 0;
}

void StringSet::add(const char *str, size_t len, Util::hash_t hash)
{
    auto size = Node::requiredSize(len);
    auto buf = m_data.add(size);
    auto node = new (buf) Node(str, len, hash);

    m_count++;
}

bool StringSet::present(const String& str) const
{
    return present(str.data(), str.length());
}

bool StringSet::present(const char *str, size_t len) const
{
    assert(m_nodes && m_count);

    auto hash = Util::hash(str, len);
    auto range = std::equal_range(m_nodes, m_nodes + m_count, hash, NodeComp());

    for (auto it = range.first; it != range.second; ++it)
        if (!it->node->removed() && !memcmp(it->node->text(), str, it->len))
            return true;

    return false;
}

void StringSet::seal()
{
    assert(!m_nodes);

    if (!m_count)
        return;

    auto buf = m_data.add(m_count * sizeof(LookupNode));
    m_nodes = reinterpret_cast<LookupNode *>(buf);

    auto node = reinterpret_cast<Node *>(m_data.begin());
    for (size_t i = 0; i < m_count; i++) {
        m_nodes[i].node = node;
        m_nodes[i].len = node->textLength();
        m_nodes[i].hash = node->hash();

        node = node->next();
    }

    std::sort(m_nodes, m_nodes + m_count);
}

auto StringSet::begin() const -> Iterator::Iterator<Node>
{
    return reinterpret_cast<Node *>(m_data.begin());
}

auto StringSet::end() const -> Iterator::Iterator<Node>
{
    return m_nodes ? reinterpret_cast<Node *>(m_nodes) : reinterpret_cast<Node *>(m_data.end());
}
