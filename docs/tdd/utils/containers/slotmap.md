# Slot Map Container Design Document

## 1. Purpose and Rationale

A slot map is a high-performance associative container that provides stable, non-intrusive handles (keys) to stored values, enabling fast insertion, removal, and lookup without invalidating existing handles except for erased elements. Slot maps are ideal for ECS, scene graphs, and resource managers where objects are frequently created and destroyed, and references must remain valid and safe.

Slot maps are described in detail in:
- [C++ Proposal P0661R0: "A Proposal to Add a Slot Map to the Standard Library"](https://open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0661r0.pdf)
- [Rust slotmap crate documentation](https://docs.rs/slotmap/latest/slotmap/)

---

## 2. Key Properties

- **Stable Handles:** Each element is referenced by a small, opaque key (handle) that remains valid until the element is removed.
- **Fast Insertion/Removal:** Insertions and removals are O(1) and do not invalidate other handles.
- **Dense Storage:** Values are stored in a contiguous array for cache efficiency and fast iteration.
- **No Pointer Indirection:** Handles are not pointers; they are small structs or integers encoding slot and generation.
- **Safe Handle Reuse:** Handles to removed elements are never reused for new elements until the slot's generation is incremented, preventing use-after-free bugs.

---

## 3. API Overview

### Key Type
A slot map key is a small struct or integer encoding:
- **Slot index:** The position in the slot array.
- **Generation:** A counter incremented on each removal to distinguish old handles from new ones.

Example (C++):
```cpp
struct SlotMapKey {
    uint32_t index;
    uint32_t generation;
};
```

### Container API
- `SlotMap<T>`: The main container type.
- `SlotMapKey insert(const T& value)`: Inserts a value, returns a key.
- `bool erase(SlotMapKey key)`: Removes the value for the key, invalidates the handle.
- `T* get(SlotMapKey key)`: Returns pointer to value if valid, else nullptr.
- `const T* get(SlotMapKey key) const`: Const version.
- `T& operator[](SlotMapKey key)`: Access value (undefined if invalid).
- `bool contains(SlotMapKey key) const`: True if key is valid.
- `void clear()`: Removes all elements, invalidates all keys.
- Iteration: Provide begin()/end() for dense value iteration (not by key).

---

## 4. Internal Data Structures

- **Slots Array:**
  - Each slot stores:
    - Index into the dense values array (if occupied)
    - Generation counter
    - Free list pointer (if vacant)
- **Values Array:**
  - Stores all live values contiguously.
  - May also store the slot index for reverse mapping.
- **Free List:**
  - Vacant slots are linked in a free list for O(1) allocation.

Example (C++):
```cpp
struct Slot {
    uint32_t value_index;   // Index into values array
    uint32_t generation;    // Generation counter
    uint32_t next_free;     // Next free slot (if vacant)
    bool occupied;          // True if slot is occupied
};
std::vector<Slot> slots;
std::vector<T> values;
std::vector<uint32_t> reverse; // values[i] -> slot index
uint32_t free_head;
```

---

## 5. Insertion and Removal

### Insertion
- Pop a slot from the free list (or grow slots array if none free).
- Store value at end of values array.
- Update slot to point to new value, set occupied=true.
- Store slot index in reverse mapping.
- Return key `{slot_index, slot.generation}`.

### Removal
- Validate key: check slot.generation matches key.generation and occupied=true.
- Swap-and-pop value from values array (to keep dense).
- Update reverse mapping for moved value.
- Mark slot as vacant, increment generation, push to free list.
- All previous keys for this slot are now invalid.

---

## 6. Lookup and Safety

- To access a value by key:
  - Check slot index and generation match, and slot is occupied.
  - If valid, use slot.value_index to access value.
  - If not, return nullptr or throw.
- Handles to removed elements are never valid for new elements until the slot's generation is incremented.

---

## 7. Iteration

- Iteration is always over the dense values array, not by key.
- No guarantee of stable order after removals (swap-and-pop).
- For each value, can provide a way to get its key if needed (store slot index in reverse mapping).

---

## 8. Example Usage

```cpp
SlotMap<Entity> entities;
SlotMapKey a = entities.insert(Entity{"Player"});
SlotMapKey b = entities.insert(Entity{"Enemy"});

entities.erase(a); // a is now invalid

if (entities.contains(b)) {
    Entity& e = entities[b];
    // ...
}

for (auto& e : entities) {
    // Iterate all live entities
}
```

---

## 9. References
- [C++ Proposal P0661R0](https://open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0661r0.pdf)
- [Rust slotmap crate](https://docs.rs/slotmap/latest/slotmap/)
- [Jonathan Blow's "Handles and Slot Maps" (YouTube)](https://www.youtube.com/watch?v=SHaAR7XPtNU)

---

This document should be updated as the slot map implementation evolves or as new requirements arise.
