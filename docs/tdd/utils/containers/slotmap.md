# SlotMap Container Documentation

## Overview

`SlotMap` is a high-performance associative container for C++ that provides stable, opaque handles (keys) to stored values. It supports fast insertion, removal, and lookup, and is ideal for use cases like ECS (Entity Component Systems), scene graphs, and resource managers where objects are frequently created and destroyed, and references must remain valid and safe.

This implementation supports multiple component types per slot, allowing you to store and retrieve several types of data for each handle.

---

## Key Features

- **Stable Handles:** Each element is referenced by a small, opaque `Handle` that remains valid until the element is removed.
- **O(1) Insertion/Removal:** Insertions and removals are constant time and do not invalidate other handles.
- **Dense Storage:** Values are stored in contiguous arrays for cache efficiency and fast iteration.
- **No Pointer Indirection:** Handles are not pointers; they encode a slot index and generation.
- **Safe Handle Reuse:** Handles to removed elements are never valid for new elements until the slot's generation is incremented, preventing use-after-free bugs.
- **Multiple Components:** Each slot can store multiple component types, e.g. `SlotMap<Position, Velocity>`.

---

## API Summary

### Handle Type

A handle is a small struct encoding:
- `slot_index`: The position in the slot array.
- `generation`: A counter incremented on each removal to distinguish old handles from new ones.

```cpp
struct Handle {
    uint32_t slot_index;
    uint32_t generation;
    // ... comparison, null, and utility methods ...
};
```

### Main API

- `SlotMap<Components...> sm;` — Create a slot map for one or more component types.
- `Handle add(Components&&... values);` — Insert a new slot with the given component values, returns a handle.
- `void remove(const Handle& h);` — Remove the slot for the given handle (no-op if invalid).
- `bool valid(const Handle& h) const;` — Returns true if the handle is valid.
- `Component* get<Component>(const Handle& h);` — Returns pointer to the component if valid, else nullptr.
- `const std::vector<Component>& raw_data<Component>() const;` — Access the dense array for a component type.

---

## Internal Data Structures

- **slots:** `std::vector<Slot>` — Each slot stores the packed index (into the dense arrays) and a generation counter.
- **free_slots:** `std::vector<uint32_t>` — Indices of vacant slots for O(1) allocation.
- **component_arrays:** `std::tuple<std::vector<Components>...>` — Dense arrays for each component type.
- **packed_to_slot:** `std::vector<uint32_t>` — Maps packed array index to slot index (for fast reverse lookup).

---

## Insertion and Removal

### Insertion
1. Pop a slot from the free list, or grow the slots array if none are free.
2. Store component values at the end of each dense array.
3. Update the slot to point to the new packed index.
4. Update `packed_to_slot` for reverse mapping.
5. Return a handle `{slot_index, slot.generation}`.

### Removal
1. Validate the handle (slot index in range, generation matches).
2. Swap-and-pop the last element in each dense array into the removed slot's packed index (to keep arrays dense).
3. Update `packed_to_slot` and the moved slot's packed index.
4. Pop the last element from each dense array and from `packed_to_slot`.
5. Increment the slot's generation and push it to the free list.

---

## Lookup and Safety

- To access a value by handle:
  - Check that the slot index is in range and the generation matches.
  - If valid, use the slot's packed index to access the value in the dense array.
  - If not valid, return nullptr.
- Handles to removed elements are never valid for new elements until the slot's generation is incremented.

---

## Iteration

- Iteration is always over the dense arrays, not by handle.
- No guarantee of stable order after removals (swap-and-pop).
- You can access the dense arrays for each component type via `raw_data<Component>()`.

---

## Example Usage

```cpp
struct Position { float x, y; };
struct Velocity { float dx, dy; };

SlotMap<Position, Velocity> sm;
auto h = sm.add(Position{1.0f, 2.0f}, Velocity{0.5f, 0.6f});

if (sm.valid(h)) {
    auto* pos = sm.get<Position>(h);
    auto* vel = sm.get<Velocity>(h);
    // ...
}

sm.remove(h); // h is now invalid

for (const auto& pos : sm.raw_data<Position>()) {
    // Iterate all live positions
}
```

---

## References
- [C++ Proposal P0661R0](https://open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0661r0.pdf)
- [Rust slotmap crate](https://docs.rs/slotmap/latest/slotmap/)
- [Jonathan Blow's "Handles and Slot Maps" (YouTube)](https://www.youtube.com/watch?v=SHaAR7XPtNU)

---

This document reflects the current implementation of `SlotMap` in `slotmap.h`. Update as the implementation evolves or new requirements arise.
