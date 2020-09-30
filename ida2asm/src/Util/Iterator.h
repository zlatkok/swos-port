#pragma once

namespace Iterator {
    template <class T>
    class Iterator
    {
    public:
        Iterator(T *t) : m_item(t) {}

        T& operator*() { return *m_item; }
        const T& operator*() const { return *m_item; }

        T *operator->() { return m_item; }
        const T *operator->() const { return m_item; }

        T *operator&() { return m_item; }
        const T *operator&() const { return m_item; }

        bool operator!=(const Iterator& rhs) const { return m_item != rhs.m_item; }
        Iterator operator++() {
            assert(m_item->next() != m_item);
            m_item = m_item->next();
            return *this;
        }
        operator const T *() const { return m_item; }

    private:
        T *m_item;
    };
}
