#pragma once

#include <vector>
#include <tuple>
#include <cassert>
#include <utility>
#include <cstdint>
#include <type_traits>


template <typename... Components>
class SlotMap {
public:
    SlotMap() = default;

    struct Handle {
        uint32_t slot_index = 0;
        uint32_t generation = 0;

        bool operator==(const Handle& other) const {
            return slot_index == other.slot_index && generation == other.generation;
        }
    };

    struct Slot {
        uint32_t packed_index = 0;
        uint32_t generation = 0;
    };

    template <typename... Args>
    Handle add(Args&&... args) {
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
        return Handle{slot_index, slots[slot_index].generation};
    }

    void remove(const Handle& handle) {
        if (!valid(handle)) {
            return; // No-op if handle is invalid
        }

        uint32_t slot_index = handle.slot_index;
        uint32_t packed_index = slots[slot_index].packed_index;
        uint32_t last_index = (uint32_t)std::get<0>(component_arrays).size() - 1;

        if (packed_index != last_index) {
            move_components(packed_index, last_index);
            uint32_t moved_slot_index = packed_to_slot[last_index];
            slots[moved_slot_index].packed_index = packed_index;
            packed_to_slot[packed_index] = moved_slot_index;
        }

        pop_components();
        packed_to_slot.pop_back();

        slots[slot_index].generation++;
        free_slots.push_back(slot_index);
    }

    bool valid(const Handle& handle) const {
        return handle.slot_index < slots.size() &&
               slots[handle.slot_index].generation == handle.generation;
    }

    template <typename Component>
    Component* get(const Handle& handle) {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");

        if (!valid(handle)) {
            return nullptr;
        }
        uint32_t packed_index = slots[handle.slot_index].packed_index;
        auto& array = std::get<std::vector<Component>>(component_arrays);
        return &array[packed_index];
    }

    template <typename Component>
    const std::vector<Component>& raw_data() const {
        static_assert(contains_type<Component, Components...>(), "Component type not found in SlotMap");
        return std::get<std::vector<Component>>(component_arrays);
    }

private:
    std::vector<Slot> slots;
    std::vector<uint32_t> free_slots;
    std::tuple<std::vector<Components>...> component_arrays;
    std::vector<uint32_t> packed_to_slot;

    // Check if type is in pack
    template <typename T, typename... Ts>
    static constexpr bool contains_type() {
        return (std::is_same_v<T, Ts> || ...);
    }

    // Insert helper
    template <typename... Args, std::size_t... Is>
    void insert_components_impl(uint32_t, std::index_sequence<Is...>, Args&&... args) {
        (std::get<Is>(component_arrays).emplace_back(std::forward<Args>(args)), ...);
    }

    template <typename... Args>
    void insert_components(uint32_t index, Args&&... args) {
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
    void move_one(Vec& vec, uint32_t dst, uint32_t src) {
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