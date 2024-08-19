#pragma once

namespace HulaScript::Hash {
    static size_t constexpr dj2b(char const* input) {
        return *input ?
            static_cast<size_t>(*input) + 33 * dj2b(input + 1) :
            5381;
    }

    //copied straight from boost
    static size_t constexpr combine(size_t lhs, size_t rhs) {
        lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
        return lhs;
    }
}