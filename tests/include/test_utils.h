#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

template <typename T> static std::vector<std::uint8_t> serialize(const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    return {reinterpret_cast<const std::uint8_t*>(&obj), reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)};
}

template <typename T> static std::vector<std::uint8_t> serialize(const std::uint8_t id, const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for byte memcpy");
    std::vector content{id};
    content.insert(
        content.end(), reinterpret_cast<const std::uint8_t*>(&obj),
        reinterpret_cast<const std::uint8_t*>(&obj) + sizeof(T)
    );
    return content;
}
