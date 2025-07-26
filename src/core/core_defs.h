#pragma once
#include "ankerl/unordered_dense.h"
#include <vector>
#include <memory>
#include <cassert>

// Type aliases for core containers and pointers
// Usage: Array<int> v; UniquePtr<MyClass> ptr;
template<typename T>
using Array = std::vector<T>;

template<typename T, std::size_t Sz>
using StaticArray = std::array<T, Sz>;

template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T, typename... Args>
inline auto make_unique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename... Args>
inline auto make_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T>
using WeakPtr = std::weak_ptr<T>;

template<typename Key, typename T>
using Map = ankerl::unordered_dense::map<Key, T>;

template<typename T>
using Set = ankerl::unordered_dense::set<T>;

// Assertion macro (crashes in debug, does nothing in release)
#define ASSERT(expr) assert(expr)

// Ensure macro: triggers a debug break if condition fails, but does not crash
#if defined(_MSC_VER)
    #define ENSURE(expr) do { if (!(expr)) { __debugbreak(); } } while(0)
#else
    #include <csignal>
    #define ENSURE(expr) do { if (!(expr)) { raise(SIGTRAP); } } while(0)
#endif
