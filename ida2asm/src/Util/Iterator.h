#pragma once

namespace Util {
    template <class T>
    class Iterator
    {
    public:
        Iterator(T *struc) : m_payload(struc) {}
        T *operator*() { return m_payload; }
        const T *operator*() const { return m_payload; }
        T *operator->() { return m_payload; }
        const T *operator->() const { return m_payload; }
        bool operator!=(const Iterator& rhs) const { return m_payload != rhs.m_payload; }
        Iterator operator++() { m_payload = m_payload->next(); return *this; }
        operator const T *() const { return m_payload; }
    private:
        T *m_payload;
    };
}
