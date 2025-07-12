#pragma once

#include <vector>
#include <tuple>
#include <cassert>
#include <utility>
#include <cstdint>

#include <type_traits>

// Forward declaration
template <typename... Components>
class SlotMap;

// SlotMapView: provides operator[] for a specific component type
template <typename Component, typename... Components>
class SlotMapView {
public:
    using Key = typename SlotMap<Components...>::Key;
    using SlotMapType = SlotMap<Components...>;

    SlotMapView(SlotMapType& in_slotmap) : slotmap(in_slotmap) {}

    const Component& operator[](const Key& key) const {
        auto* ptr = slotmap.get<Component>(key);
        assert(ptr && "Invalid key access in SlotMapView");
        return *ptr;
    }
    
    Component& operator[](const Key& key) {
        return const_cast<Component&>(
            static_cast<const SlotMapView*>(this)->operator[](key)
        );
    }

    // Optionally, expose raw data
    const std::vector<Component>& raw_data() const {
        return slotmap.template raw_data<Component>();
    }

private:
    SlotMapType& slotmap;
};

template <typename... Components>
class SlotMap {
public:
    SlotMap() = default;
    // Get a view for a specific component type
    template <typename Component>
    SlotMapView<Component, Components...> view() {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");
        return SlotMapView<Component, Components...>(*this);
    }

    template <typename Component>
    const SlotMapView<Component, Components...> view() const {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");
        return SlotMapView<Component, Components...>(const_cast<SlotMap&>(*this));
    }

    struct Key {
        uint32_t slot_index = 0xFFFFFFFF;
        uint32_t generation = 0xFFFFFFFF;

        bool operator==(const Key& other) const {
            return slot_index == other.slot_index && generation == other.generation;
        }

        bool operator!=(const Key& other) const {
            return !(*this == other);
        }

        bool operator<(const Key& other) const {
            return std::tie(slot_index, generation) < std::tie(other.slot_index, other.generation);
        }

        bool is_null() const {
            return slot_index == 0xFFFFFFFF && generation == 0xFFFFFFFF;
        }

        explicit operator bool() const {
            return !is_null();
        }

        static constexpr Key null() noexcept {
            return Key{0xFFFFFFFF, 0xFFFFFFFF};
        }
    };

    struct Slot {
        uint32_t packed_index = 0;
        uint32_t generation   = 0;
    };

    template <typename... Args>
    Key add(Args &&...args) {
        static_assert(sizeof...(Components) == sizeof...(Args), "Number of arguments must match number of components");

        uint32_t slot_index;
        if (!free_slots.empty()) {
            slot_index = free_slots.back();
            free_slots.pop_back();
        } else {
            slot_index = (uint32_t)slots.size();
            slots.push_back(Slot{});
        }

        uint32_t packed_index = (uint32_t)std::get<0>(component_arrays).size();

        insert_components(packed_index, std::forward<Args>(args)...);

        slots[slot_index].packed_index = packed_index;
        packed_to_slot.push_back(slot_index);
        return Key{slot_index, slots[slot_index].generation};
    }

    void remove(const Key& key) {
        if (!is_valid(key)) {
            return; // No-op if key is invalid
        }

        uint32_t slot_index   = key.slot_index;
        uint32_t packed_index = slots[slot_index].packed_index;
        uint32_t last_index   = (uint32_t)std::get<0>(component_arrays).size() - 1;

        if (packed_index != last_index) {
            move_components(packed_index, last_index);
            uint32_t moved_slot_index            = packed_to_slot[last_index];
            slots[moved_slot_index].packed_index = packed_index;
            packed_to_slot[packed_index]         = moved_slot_index;
        }

        pop_components();
        packed_to_slot.pop_back();

        slots[slot_index].generation++;
        free_slots.push_back(slot_index);
    }

    bool is_valid(const Key& key) const {
        return key.slot_index < slots.size() && slots[key.slot_index].generation == key.generation;
    }

    template <typename Component>
    const Component* get(const Key& key) const {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");

        if (!is_valid(key)) {
            return nullptr;
        }
        uint32_t packed_index = slots[key.slot_index].packed_index;
        const auto& array     = std::get<std::vector<Component>>(component_arrays);
        return &array[packed_index];
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
    const std::vector<Component>& raw_data() const {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");
        return std::get<std::vector<Component>>(component_arrays);
    }

private:
    std::vector<Slot>                      slots;
    std::vector<uint32_t>                  free_slots;
    std::tuple<std::vector<Components>...> component_arrays;
    std::vector<uint32_t>                  packed_to_slot;

    // Check if type is in pack
    template <typename T, typename... Ts>
    static constexpr bool contains_type() {
        return (std::is_same_v<T, Ts> || ...);
    }

    // Insert helper
    template <typename... Args, std::size_t... Is>
    void insert_components_impl(uint32_t, std::index_sequence<Is...>, Args &&...args) {
        (std::get<Is>(component_arrays).emplace_back(std::forward<Args>(args)), ...);
    }

    template <typename... Args>
    void insert_components(uint32_t index, Args &&...args) {
        insert_components_impl(index, std::index_sequence_for<Components...>{}, std::forward<Args>(args)...);
    }

    // Move helper
    template <std::size_t... Is>
    void move_components_impl(uint32_t dst, uint32_t src, std::index_sequence<Is...>) {
        (move_one(std::get<Is>(component_arrays), dst, src), ...);
    }

    void move_components(uint32_t dst, uint32_t src) {
        move_components_impl(dst, src, std::index_sequence_for<Components...>{});
    }

    template <typename Vec>
    void move_one(Vec &vec, uint32_t dst, uint32_t src) {
        vec[dst] = std::move(vec[src]);
    }

    // Pop helper
    template <std::size_t... Is>
    void pop_components_impl(std::index_sequence<Is...>) {
        (std::get<Is>(component_arrays).pop_back(), ...);
    }

    void pop_components() {
        pop_components_impl(std::index_sequence_for<Components...>{});
    }
};

#define DEFINE_SLOTMAP_KEY_HASH(KeyType) \
template <> \
struct ankerl::unordered_dense::hash<KeyType> { \
    using is_avalanching = void; \
    [[nodiscard]] auto operator()(const KeyType& h) const noexcept -> uint64_t { \
        return (static_cast<uint64_t>(h.slot_index) << 32) | h.generation; \
    } \
};
