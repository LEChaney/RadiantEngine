#pragma once

#include "ankerl/unordered_dense.h"

#include <vector>
#include <tuple>
#include <cassert>
#include <utility>
#include <cstdint>
#include <type_traits>

// Forward declaration
template <typename... Components>
class SlotMap;



// Generic key-value iterator for SlotMap and SlotMapView
template <typename Container, typename Component>
class SlotMapKvIterator {
public:
    using Key = typename Container::Key;
    using difference_type = std::ptrdiff_t;
    using ReferenceType = decltype(std::declval<Container>().template rawData<Component>()[0]);
    using ValueType = std::pair<Key, ReferenceType>;
    using Pointer = ValueType*;
    using Reference = ValueType;
    using IteratorCategory = std::forward_iterator_tag;

    SlotMapKvIterator(Container& container, size_t idx)
        : m_container(container), m_idx(idx) {}

    Reference operator*() const {
        uint32_t slotIndex = m_container.m_packedToSlot[m_idx];
        Key key{slotIndex, m_container.m_slots[slotIndex].m_generation};
        return ValueType{key, m_container.template rawData<Component>()[m_idx]};
    }
    SlotMapKvIterator& operator++() { ++m_idx; return *this; }
    SlotMapKvIterator operator++(int) { SlotMapKvIterator tmp = *this; ++m_idx; return tmp; }
    bool operator==(const SlotMapKvIterator& other) const { return m_idx == other.m_idx; }
    bool operator!=(const SlotMapKvIterator& other) const { return m_idx != other.m_idx; }
private:
    Container& m_container;
    size_t m_idx;
};

// SlotMapView: provides operator[] for a specific component type
template <typename Component, typename... Components>
class SlotMapView {
public:
    using Key = typename SlotMap<Components...>::Key;
    using SlotMapType = SlotMap<Components...>;

    SlotMapView(SlotMapType& slotMap) : m_slotMap(slotMap) {}

    const Component& operator[](const Key& key) const {
        auto* ptr = m_slotMap.get<Component>(key);
        assert(ptr && "Invalid key access in SlotMapView");
        return *ptr;
    }
    
    Component& operator[](const Key& key) {
        return const_cast<Component&>(
            static_cast<const SlotMapView*>(this)->operator[](key)
        );
    }

    // Optionally, expose raw data
    const std::vector<Component>& rawData() const {
        return m_slotMap.template rawData<Component>();
    }

    // Range-based for loop support (key-value by default)
    auto begin() { return SlotMapKvIterator<SlotMapType, Component>(m_slotMap, 0); }
    auto end()   { return SlotMapKvIterator<SlotMapType, Component>(m_slotMap, m_slotMap.template rawData<Component>().size()); }
    auto begin() const { return SlotMapKvIterator<SlotMapType, Component>(const_cast<SlotMapType&>(m_slotMap), 0); }
    auto end()   const { return SlotMapKvIterator<SlotMapType, Component>(const_cast<SlotMapType&>(m_slotMap), m_slotMap.template rawData<Component>().size()); }

    // Raw data iteration
    auto rawBegin() { return m_slotMap.template rawData<Component>().begin(); }
    auto rawEnd()   { return m_slotMap.template rawData<Component>().end(); }
    auto rawBegin() const { return m_slotMap.template rawData<Component>().begin(); }
    auto rawEnd()   const { return m_slotMap.template rawData<Component>().end(); }

    // Key-value iterator support (explicit)
    auto kvBegin() { return begin(); }
    auto kvEnd()   { return end(); }

private:
    SlotMapType& m_slotMap;
};

template <typename... Components>
class SlotMap {
public:
    SlotMap() = default;

    // Allow SlotMapView and iterator to access private members
    template <typename Component, typename... Cs>
    friend class SlotMapView;
    template <typename Container, typename ComponentT>
    friend class SlotMapKvIterator;

    struct Key {
        uint32_t m_slotIndex = 0xFFFFFFFF;
        uint32_t m_generation = 0xFFFFFFFF;

        bool operator==(const Key& other) const {
            return m_slotIndex == other.m_slotIndex && m_generation == other.m_generation;
        }

        bool operator!=(const Key& other) const {
            return !(*this == other);
        }

        bool operator<(const Key& other) const {
            return std::tie(m_slotIndex, m_generation) < std::tie(other.m_slotIndex, other.m_generation);
        }

        bool isNull() const {
            return m_slotIndex == 0xFFFFFFFF && m_generation == 0xFFFFFFFF;
        }

        explicit operator bool() const {
            return !isNull();
        }

        static constexpr Key null() noexcept {
            return Key{0xFFFFFFFF, 0xFFFFFFFF};
        }
    };

    struct Slot {
        uint32_t m_packedIndex = 0;
        uint32_t m_generation   = 0;
    };

    // Add method: accepts any subset of component arguments, missing types are default constructed
    template <typename... Args>
    Key add(Args&&... args) {
        static_assert(sizeof...(Args) <= sizeof...(Components), "Too many arguments for SlotMap::add");
        uint32_t slotIndex;
        if (!m_freeSlots.empty()) {
            slotIndex = m_freeSlots.back();
            m_freeSlots.pop_back();
        } else {
            slotIndex = (uint32_t)m_slots.size();
            m_slots.push_back(Slot{});
        }

        uint32_t packedIndex = (uint32_t)std::get<0>(m_componentArrays).size();

        insertOrDefaultComponents(std::forward<Args>(args)...);

        m_slots[slotIndex].m_packedIndex = packedIndex;
        m_packedToSlot.push_back(slotIndex);
        return Key{slotIndex, m_slots[slotIndex].m_generation};
    }

    void remove(const Key& key) {
        if (!isValid(key)) {
            return; // No-op if key is invalid
        }

        uint32_t slotIndex   = key.m_slotIndex;
        uint32_t packedIndex = m_slots[slotIndex].m_packedIndex;
        uint32_t lastIndex   = (uint32_t)std::get<0>(m_componentArrays).size() - 1;

        if (packedIndex != lastIndex) {
            moveComponents(packedIndex, lastIndex);
            uint32_t movedSlotIndex            = m_packedToSlot[lastIndex];
            m_slots[movedSlotIndex].m_packedIndex = packedIndex;
            m_packedToSlot[packedIndex]         = movedSlotIndex;
        }

        popComponents();
        m_packedToSlot.pop_back();

        m_slots[slotIndex].m_generation++;
        m_freeSlots.push_back(slotIndex);
    }

    bool isValid(const Key& key) const {
        return key.m_slotIndex < m_slots.size() && m_slots[key.m_slotIndex].m_generation == key.m_generation;
    }

    template <typename Component>
    const Component* get(const Key& key) const {
        static_assert(containsType<Component, Components...>(), "Component type not found in SlotMap");

        if (!isValid(key)) {
            return nullptr;
        }
        uint32_t packedIndex = m_slots[key.m_slotIndex].m_packedIndex;
        const auto& array     = std::get<std::vector<Component>>(m_componentArrays);
        return &array[packedIndex];
    }

    template <typename Component>
    Component* get(const Key& key) {
        return const_cast<Component*>(static_cast<const SlotMap*>(this)->get<Component>(key));
    }
    
    const std::tuple_element_t<0, std::tuple<Components...>>& operator[](const Key& key) const {
        using Component = std::tuple_element_t<0, std::tuple<Components...>>;
        auto* ptr = get<Component>(key);
        assert(ptr && "Invalid key access in SlotMap");
        return *ptr;
    }

    std::tuple_element_t<0, std::tuple<Components...>>& operator[](const Key& key) {
        return const_cast<std::tuple_element_t<0, std::tuple<Components...>>&>(
            static_cast<const SlotMap*>(this)->operator[](key)
        );
    }

    template <typename Component>
    const std::vector<Component>& rawData() const {
        static_assert(containsType<Component, Components...>(), "Component type not found in SlotMap");
        return std::get<std::vector<Component>>(m_componentArrays);
    }

    // Get a view for a specific component type
    template <typename Component>
    SlotMapView<Component, Components...> view() {
        static_assert(containsType<Component, Components...>(), "Component type not found in SlotMap");
        return SlotMapView<Component, Components...>(*this);
    }

    template <typename Component>
    const SlotMapView<Component, Components...> view() const {
        static_assert(containsType<Component, Components...>(), "Component type not found in SlotMap");
        return SlotMapView<Component, Components...>(const_cast<SlotMap&>(*this));
    }

    // Range-based for loop support (key-value by default)
    auto begin() {
        using Component = std::tuple_element_t<0, std::tuple<Components...>>;
        return SlotMapKvIterator<SlotMap, Component>(*this, 0);
    }
    auto end() {
        using Component = std::tuple_element_t<0, std::tuple<Components...>>;
        return SlotMapKvIterator<SlotMap, Component>(*this, std::get<0>(m_componentArrays).size());
    }
    auto begin() const {
        using Component = std::tuple_element_t<0, std::tuple<Components...>>;
        return SlotMapKvIterator<SlotMap, Component>(const_cast<SlotMap&>(*this), 0);
    }
    auto end() const {
        using Component = std::tuple_element_t<0, std::tuple<Components...>>;
        return SlotMapKvIterator<SlotMap, Component>(const_cast<SlotMap&>(*this), std::get<0>(m_componentArrays).size());
    }

    // Raw data iteration
    auto rawBegin() { return std::get<0>(m_componentArrays).begin(); }
    auto rawEnd()   { return std::get<0>(m_componentArrays).end(); }
    auto rawBegin() const { return std::get<0>(m_componentArrays).begin(); }
    auto rawEnd()   const { return std::get<0>(m_componentArrays).end(); }

    // Key-value iterator support (explicit)
    auto kvBegin() { return begin(); }
    auto kvEnd()   { return end(); }

private:
    std::vector<Slot>                      m_slots;
    std::vector<uint32_t>                  m_freeSlots;
    std::tuple<std::vector<Components>...> m_componentArrays;
    std::vector<uint32_t>                  m_packedToSlot;

    // Check if type is in pack
    template <typename T, typename... Ts>
    static constexpr bool containsType() {
        return (std::is_same_v<T, Ts> || ...);
    }
    // Helper: emplace provided args, default construct missing types
    template <typename... Args, std::size_t... Is>
    void insertOrDefaultComponentsImpl(std::index_sequence<Is...>, Args&&... args) {
        (insertOneComponent<Is, Args...>(std::forward<Args>(args)...), ...);
    }
    template <typename... Args>
    void insertOrDefaultComponents(Args&&... args) {
        insertOrDefaultComponentsImpl(std::make_index_sequence<sizeof...(Components)>(), std::forward<Args>(args)...);
    }
    template <std::size_t I, typename... Args>
    void insertOneComponent(Args&&... args) {
        using Component = std::tuple_element_t<I, std::tuple<Components...>>;
        if constexpr (I < sizeof...(Args)) {
            std::get<I>(m_componentArrays).emplace_back(std::get<I>(std::forward_as_tuple(std::forward<Args>(args)...)));
        } else {
            std::get<I>(m_componentArrays).emplace_back();
        }
    }
    // Move helper
    template <std::size_t... Is>
    void moveComponentsImpl(uint32_t dst, uint32_t src, std::index_sequence<Is...>) {
        (moveOne(std::get<Is>(m_componentArrays), dst, src), ...);
    }
    void moveComponents(uint32_t dst, uint32_t src) {
        moveComponentsImpl(dst, src, std::index_sequence_for<Components...>{});
    }
    template <typename Vec>
    void moveOne(Vec &vec, uint32_t dst, uint32_t src) {
        vec[dst] = std::move(vec[src]);
    }
    // Pop helper
    template <std::size_t... Is>
    void popComponentsImpl(std::index_sequence<Is...>) {
        (std::get<Is>(m_componentArrays).pop_back(), ...);
    }
    void popComponents() {
        popComponentsImpl(std::index_sequence_for<Components...>{});
    }
};

template <typename KeyType>
struct ankerl::unordered_dense::hash<KeyType> {
    using is_avalanching = void;
    auto operator()(const KeyType& h) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<uint64_t>()(static_cast<uint64_t>(h.m_generation) << 32 | h.m_slotIndex);
    }
};
