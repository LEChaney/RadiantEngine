#include "utils/containers/slotmap.h"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

struct Position { float x, y; };
struct Velocity { float dx, dy; };

TEST(SlotMapTest, AddAndGetSingleComponent) {
    SlotMap<Position> sm;
    auto h = sm.add(Position{1.0f, 2.0f});
    ASSERT_TRUE(sm.valid(h));
    auto* pos = sm.get<Position>(h);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
}

TEST(SlotMapTest, AddAndGetMultipleComponents) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add(Position{1.0f, 2.0f}, Velocity{0.5f, 0.6f});
    ASSERT_TRUE(sm.valid(h));
    auto* pos = sm.get<Position>(h);
    auto* vel = sm.get<Velocity>(h);
    ASSERT_NE(pos, nullptr);
    ASSERT_NE(vel, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(vel->dx, 0.5f);
}

TEST(SlotMapTest, RemoveInvalidatesHandle) {
    SlotMap<Position> sm;
    auto h = sm.add(Position{1.0f, 2.0f});
    sm.remove(h);
    EXPECT_FALSE(sm.valid(h));
    EXPECT_EQ(sm.get<Position>(h), nullptr);
}

TEST(SlotMapTest, ReuseSlotAfterRemove) {
    SlotMap<Position> sm;
    auto h1 = sm.add(Position{1.0f, 2.0f});
    sm.remove(h1);
    auto h2 = sm.add(Position{3.0f, 4.0f});
    ASSERT_TRUE(sm.valid(h2));
    auto* pos = sm.get<Position>(h2);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 3.0f);
    EXPECT_FLOAT_EQ(pos->y, 4.0f);
    EXPECT_FALSE(sm.valid(h1));
}

TEST(SlotMapTest, RawDataAccess) {
    SlotMap<Position, Velocity> sm;
    sm.add(Position{1, 2}, Velocity{3, 4});
    sm.add(Position{5, 6}, Velocity{7, 8});
    const auto& positions = sm.raw_data<Position>();
    const auto& velocities = sm.raw_data<Velocity>();
    ASSERT_EQ(positions.size(), 2u);
    ASSERT_EQ(velocities.size(), 2u);
    EXPECT_EQ(positions[0].x, 1);
    EXPECT_EQ(velocities[1].dy, 8);
}

TEST(SlotMapTest, HandleValidityAfterMultipleAddRemove) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1, 2}, Velocity{3, 4});
    auto h2 = sm.add(Position{5, 6}, Velocity{7, 8});
    auto h3 = sm.add(Position{9, 10}, Velocity{11, 12});

    // All handles should be valid
    ASSERT_TRUE(sm.valid(h1));
    ASSERT_TRUE(sm.valid(h2));
    ASSERT_TRUE(sm.valid(h3));

    // Remove the second handle
    sm.remove(h2);
    EXPECT_FALSE(sm.valid(h2));
    EXPECT_EQ(sm.get<Position>(h2), nullptr);
    EXPECT_EQ(sm.get<Velocity>(h2), nullptr);

    // h1 and h3 should still be valid and have correct values
    auto* pos1 = sm.get<Position>(h1);
    auto* vel1 = sm.get<Velocity>(h1);
    ASSERT_NE(pos1, nullptr);
    ASSERT_NE(vel1, nullptr);
    EXPECT_EQ(pos1->x, 1);
    EXPECT_EQ(pos1->y, 2);
    EXPECT_EQ(vel1->dx, 3);
    EXPECT_EQ(vel1->dy, 4);

    auto* pos3 = sm.get<Position>(h3);
    auto* vel3 = sm.get<Velocity>(h3);
    ASSERT_NE(pos3, nullptr);
    ASSERT_NE(vel3, nullptr);
    EXPECT_EQ(pos3->x, 9);
    EXPECT_EQ(pos3->y, 10);
    EXPECT_EQ(vel3->dx, 11);
    EXPECT_EQ(vel3->dy, 12);

    // Remove h1, check validity
    sm.remove(h1);
    EXPECT_FALSE(sm.valid(h1));
    EXPECT_EQ(sm.get<Position>(h1), nullptr);
    EXPECT_EQ(sm.get<Velocity>(h1), nullptr);

    // h3 should still be valid
    ASSERT_TRUE(sm.valid(h3));
    pos3 = sm.get<Position>(h3);
    vel3 = sm.get<Velocity>(h3);
    ASSERT_NE(pos3, nullptr);
    ASSERT_NE(vel3, nullptr);
    EXPECT_EQ(pos3->x, 9);
    EXPECT_EQ(pos3->y, 10);
    EXPECT_EQ(vel3->dx, 11);
    EXPECT_EQ(vel3->dy, 12);
}

TEST(SlotMapTest, RemoveInvalidHandleIsNoOp) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1, 2}, Velocity{3, 4});
    auto h2 = sm.add(Position{5, 6}, Velocity{7, 8});
    sm.remove(h1);
    // h1 is now invalid
    EXPECT_FALSE(sm.valid(h1));
    // Removing again should not throw or assert (should be a no-op)
    sm.remove(h1);
    // h2 should still be valid
    EXPECT_TRUE(sm.valid(h2));
    auto* pos2 = sm.get<Position>(h2);
    auto* vel2 = sm.get<Velocity>(h2);
    ASSERT_NE(pos2, nullptr);
    ASSERT_NE(vel2, nullptr);
    EXPECT_EQ(pos2->x, 5);
    EXPECT_EQ(pos2->y, 6);
    EXPECT_EQ(vel2->dx, 7);
    EXPECT_EQ(vel2->dy, 8);
}

TEST(SlotMapTest, NullHandleProperties) {
    using Handle = SlotMap<Position>::Handle;
    // Null handle should be equal to itself
    EXPECT_EQ(Handle::null(), Handle::null());
    // Null handle should not be valid in any slotmap
    SlotMap<Position> sm;
    auto h = sm.add(Position{1, 2});
    EXPECT_FALSE(sm.valid(Handle::null()));
    // Null handle should be detected by is_null()
    EXPECT_TRUE(Handle::null().is_null());
    // Null handle should convert to false in boolean context
    EXPECT_FALSE(static_cast<bool>(Handle::null()));
    // Null handle should not be equal to a valid handle
    EXPECT_NE(h, Handle::null());
    // Null handle should be trivially copyable and constexpr
    static_assert(std::is_trivially_copyable<Handle>::value, "Handle should be trivially copyable");
    constexpr Handle nh = Handle::null();
    (void)nh;
}