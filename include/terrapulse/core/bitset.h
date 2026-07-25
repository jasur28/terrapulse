#pragma once

#include <vector>

namespace tp::core {

class BitSet {
public:
    explicit BitSet(int size = 0) : m_bits(size, false) {}

    int size() const { return static_cast<int>(m_bits.size()); }
    void resize(int size) { m_bits.resize(size, false); }
    bool test(int index) const { return index >= 0 && index < size() ? m_bits[static_cast<std::size_t>(index)] : false; }
    void set(int index, bool value = true) {
        if (index >= 0 && index < size()) m_bits[static_cast<std::size_t>(index)] = value;
    }

private:
    std::vector<bool> m_bits;
};

} // namespace tp::core
