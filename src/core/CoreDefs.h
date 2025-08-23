#pragma once
#include "ankerl/unordered_dense.h"
#include "ankerl/svector.h"
#include <vector>
#include <memory>
#include <utility>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <type_traits>

#if defined(_WIN32)
#define NOMINMAX
    #include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <unistd.h>
    #include <limits.h>
#endif


// Integer typedefs
using int8   = int8_t;
using int16  = int16_t;
using int32  = int32_t;
using int64  = int64_t;
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using Path = std::filesystem::path;

// Core containers / pointers
template<typename T>
using Array = std::vector<T>;

template<typename T, std::size_t sz>
using SmallArray = ankerl::svector<T, sz>;

template<typename T, std::size_t sz>
using StaticArray = std::array<T, sz>;

template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T, typename... Args>
inline auto makeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename... Args>
inline auto makeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T>
using WeakPtr = std::weak_ptr<T>;

template<typename Key, typename T>
using Map = ankerl::unordered_dense::map<Key, T>;

template<typename T>
using Set = ankerl::unordered_dense::set<T>;

template<typename T>
using InitializerList = std::initializer_list<T>;

// Cast helpers (verbosity reduction)
// static_cast wrapper
template<class To, class From>
constexpr To cast(From&& value) noexcept {
    return static_cast<To>(std::forward<From>(value));
}

// dynamic_cast wrapper (pointer overload)
template<class To, class From>
To dyncast(From* ptr) noexcept { // returns nullptr on failure when To is a pointer
    return dynamic_cast<To>(ptr);
}

// dynamic_cast wrapper (reference overload) - may throw std::bad_cast
template<class To, class From>
To dyncast(From& ref) { // no noexcept: dynamic_cast on references can throw
    return dynamic_cast<To>(ref);
}

// reinterpret_cast wrapper
template<class To, class From>
constexpr To rcast(From&& value) noexcept {
    return reinterpret_cast<To>(std::forward<From>(value));
}

// Optional: bounds-checked narrowing cast (asserts in debug, returns value in release)
template<class To, class From>
constexpr To narrow(From value) {
    To out = static_cast<To>(value);
#if !defined(NDEBUG)
    // If round-trip changes the value, it's a narrowing error
    ASSERT(static_cast<From>(out) == value && "narrow(): data loss");
#endif
    return out;
}

// Generic flags wrapper
template<typename Enum>
class Flags {
public:
    using Underlying = std::underlying_type_t<Enum>;

    constexpr Flags() : m_bits(0) {}
    constexpr Flags(Underlying bits) : m_bits(bits) {}

    // Implicit conversion from Enum to Flags
    constexpr Flags& operator=(Enum bit) { m_bits = static_cast<Underlying>(bit); return *this; }
    constexpr Flags(const Enum& bit) : m_bits(static_cast<Underlying>(bit)) {}

    constexpr Flags operator|(Enum bit) const { return Flags(m_bits | static_cast<Underlying>(bit)); }
    constexpr Flags operator|(Flags other) const { return Flags(m_bits | other.m_bits); }
    constexpr Flags operator&(Enum bit) const { return Flags(m_bits & static_cast<Underlying>(bit)); }
    constexpr Flags operator&(Flags other) const { return Flags(m_bits & other.m_bits); }
    constexpr Flags operator~() const { return Flags(~m_bits); }

    Flags& operator|=(Enum bit) { m_bits |= static_cast<Underlying>(bit); return *this; }
    Flags& operator|=(Flags other) { m_bits |= other.m_bits; return *this; }
    Flags& operator&=(Enum bit) { m_bits &= static_cast<Underlying>(bit); return *this; }
    Flags& operator&=(Flags other) { m_bits &= other.m_bits; return *this; }

    constexpr bool operator==(Flags other) const { return m_bits == other.m_bits; }
    constexpr bool operator!=(Flags other) const { return m_bits != other.m_bits; }
    constexpr explicit operator bool() const { return m_bits != 0; }
    constexpr Underlying bits() const { return m_bits; }

    constexpr bool hasFlag(Enum bit) const { return (m_bits & static_cast<Underlying>(bit)) != 0; }
    constexpr bool hasAnyFlags(Flags other) const { return (m_bits & other.m_bits) != 0; }
    constexpr bool hasAllFlags(Flags other) const { return (m_bits & other.m_bits) == other.m_bits; }

private:
    Underlying m_bits;
};

// Helper macro for defining flag types
#define DECLARE_FLAGS(EnumType, FlagsType) \
    using FlagsType = Flags<EnumType>; \
    inline FlagsType operator|(EnumType a, EnumType b) { return FlagsType(a) | b; } \
    inline FlagsType operator&(EnumType a, EnumType b) { return FlagsType(a) & b; }

// Portable DEBUG_BREAK (always available)
#ifndef DEBUG_BREAK
    #if defined(_MSC_VER)
        #define DEBUG_BREAK() __debugbreak()
    #elif defined(__GNUC__) || defined(__clang__)
        #if defined(__i386__) || defined(__x86_64__)
            #define DEBUG_BREAK() __asm__ volatile("int3")
        #else
            #include <csignal>
            #define DEBUG_BREAK() raise(SIGTRAP)
        #endif
    #else
        #include <csignal>
        #define DEBUG_BREAK() raise(SIGTRAP)
    #endif
#endif

// ASSERT: active only in debug
#if !defined(NDEBUG)
    #define ASSERT(expr) do { if(!(expr)) { DEBUG_BREAK(); assert(expr); } } while(0)
#else
    #define ASSERT(expr) ((void)0)
#endif

// ENSURE: always triggers a break (non-crashing) when condition fails
#if !defined(NDEBUG)
    #define ENSURE(expr) do { if(!(expr)) { DEBUG_BREAK(); } } while(0)
#else
    #define ENSURE(expr) ((void)0)
#endif

// Get the directory containing the current executable
inline std::filesystem::path getExecutablePath() {
#if defined(_WIN32)
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#elif defined(__linux__) || defined(__APPLE__)
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    return std::filesystem::path(std::string(path, (count > 0) ? count : 0)).parent_path();
#else
    #error "Unsupported platform"
#endif
}
