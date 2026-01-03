#ifndef CORE_CACHE_ANYKEY_H
#define CORE_CACHE_ANYKEY_H

#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <typeindex>

namespace core::cache {

template <typename T>
concept Hashable = requires(const T& v) {
    { std::hash<T>{}(v) } -> std::convertible_to<std::size_t>;
};

struct AnyKey {
    std::type_index type;
    std::size_t hash;

    bool operator==(const AnyKey& other) const noexcept {
        return type == other.type && hash == other.hash;
    }

    template <Hashable T>
    static AnyKey make(const T& key) noexcept {
        return AnyKey{.type = typeid(T), .hash = std::hash<T>{}(key)};
    }
};

}  // namespace core::cache

namespace std {
template <>
struct hash<core::cache::AnyKey> {
    size_t operator()(const core::cache::AnyKey& k) const noexcept {
        return k.hash ^ (std::hash<std::type_index>{}(k.type) << 1);
    }
};
}  // namespace std

#endif  // CORE_CACHE_ANYKEY_H
