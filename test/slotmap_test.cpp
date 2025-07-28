#include "utils/containers/slotmap.h"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

struct Position { float x=0, y=0; };
struct Velocity { float dx=0, dy=0; };

TEST(SlotMapTest, AddAndGetSingleComponent) {
    SlotMap<Position> sm;
    auto h = sm.add(Position{1.0f, 2.0f});
    ASSERT_TRUE(sm.isValid(h));
    auto* pos = sm.get<Position>(h);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
}

TEST(SlotMapTest, AddAndGetMultipleComponents) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add(Position{1.0f, 2.0f}, Velocity{0.5f, 0.6f});
    ASSERT_TRUE(sm.isValid(h));
    auto* pos = sm.get<Position>(h);
    auto* vel = sm.get<Velocity>(h);
    ASSERT_NE(pos, nullptr);
    ASSERT_NE(vel, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(vel->dx, 0.5f);
}

TEST(SlotMapTest, RemoveInvalidatesKey) {
    SlotMap<Position> sm;
    auto h = sm.add(Position{1.0f, 2.0f});
    sm.remove(h);
    EXPECT_FALSE(sm.isValid(h));
    EXPECT_EQ(sm.get<Position>(h), nullptr);
}

TEST(SlotMapTest, ReuseSlotAfterRemove) {
    SlotMap<Position> sm;
    auto h1 = sm.add(Position{1.0f, 2.0f});
    sm.remove(h1);
    auto h2 = sm.add(Position{3.0f, 4.0f});
    ASSERT_TRUE(sm.isValid(h2));
    auto* pos = sm.get<Position>(h2);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 3.0f);
    EXPECT_FLOAT_EQ(pos->y, 4.0f);
    EXPECT_FALSE(sm.isValid(h1));
}

TEST(SlotMapTest, RawDataAccess) {
    SlotMap<Position, Velocity> sm;
    sm.add(Position{1, 2}, Velocity{3, 4});
    sm.add(Position{5, 6}, Velocity{7, 8});
    const auto& positions = sm.rawData<Position>();
    const auto& velocities = sm.rawData<Velocity>();
    ASSERT_EQ(positions.size(), 2u);
    ASSERT_EQ(velocities.size(), 2u);
    EXPECT_EQ(positions[0].x, 1);
    EXPECT_EQ(velocities[1].dy, 8);
}

TEST(SlotMapTest, KeyValidityAfterMultipleAddRemove) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1, 2}, Velocity{3, 4});
    auto h2 = sm.add(Position{5, 6}, Velocity{7, 8});
    auto h3 = sm.add(Position{9, 10}, Velocity{11, 12});

    // All keys should be valid
    ASSERT_TRUE(sm.isValid(h1));
    ASSERT_TRUE(sm.isValid(h2));
    ASSERT_TRUE(sm.isValid(h3));

    // Remove the second key
    sm.remove(h2);
    EXPECT_FALSE(sm.isValid(h2));
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
    EXPECT_FALSE(sm.isValid(h1));
    EXPECT_EQ(sm.get<Position>(h1), nullptr);
    EXPECT_EQ(sm.get<Velocity>(h1), nullptr);

    // h3 should still be valid
    ASSERT_TRUE(sm.isValid(h3));
    pos3 = sm.get<Position>(h3);
    vel3 = sm.get<Velocity>(h3);
    ASSERT_NE(pos3, nullptr);
    ASSERT_NE(vel3, nullptr);
    EXPECT_EQ(pos3->x, 9);
    EXPECT_EQ(pos3->y, 10);
    EXPECT_EQ(vel3->dx, 11);
    EXPECT_EQ(vel3->dy, 12);
}

TEST(SlotMapTest, RemoveInvalidKeyIsNoOp) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1, 2}, Velocity{3, 4});
    auto h2 = sm.add(Position{5, 6}, Velocity{7, 8});
    sm.remove(h1);
    // h1 is now invalid
    EXPECT_FALSE(sm.isValid(h1));
    // Removing again should not throw or assert (should be a no-op)
    sm.remove(h1);
    // h2 should still be valid
    EXPECT_TRUE(sm.isValid(h2));
    auto* pos2 = sm.get<Position>(h2);
    auto* vel2 = sm.get<Velocity>(h2);
    ASSERT_NE(pos2, nullptr);
    ASSERT_NE(vel2, nullptr);
    EXPECT_EQ(pos2->x, 5);
    EXPECT_EQ(pos2->y, 6);
    EXPECT_EQ(vel2->dx, 7);
    EXPECT_EQ(vel2->dy, 8);
}

TEST(SlotMapTest, NullKeyProperties) {
    using Key = SlotMap<Position>::Key;
    // Null key should be equal to itself
    EXPECT_EQ(Key::null(), Key::null());
    // Null key should not be valid in any slotmap
    SlotMap<Position> sm;
    auto h = sm.add(Position{1, 2});
    EXPECT_FALSE(sm.isValid(Key::null()));
    // Null key should be detected by isNull()
    EXPECT_TRUE(Key::null().isNull());
    // Null key should convert to false in boolean context
    EXPECT_FALSE(static_cast<bool>(Key::null()));
    // Null key should not be equal to a valid key
    EXPECT_NE(h, Key::null());
    // Null key should be trivially copyable and constexpr
    static_assert(std::is_trivially_copyable_v<Key>, "Key should be trivially copyable");
    constexpr Key nh = Key::null();
    (void)nh;
}

TEST(SlotMapTest, SlotMapViewSingleComponent) {
    SlotMap<Position> sm;
    auto h = sm.add(Position{10.0f, 20.0f});
    ASSERT_TRUE(sm.isValid(h));
    auto view = sm.view<Position>();
    Position& pos = view[h];
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 20.0f);
    // Const version
    const auto& csm = sm;
    auto cview = csm.view<Position>();
    const Position& cpos = cview[h];
    EXPECT_FLOAT_EQ(cpos.x, 10.0f);
    EXPECT_FLOAT_EQ(cpos.y, 20.0f);
    // Raw data
    const auto& data = view.rawData();
    ASSERT_EQ(data.size(), 1u);
    EXPECT_FLOAT_EQ(data[0].x, 10.0f);
    EXPECT_FLOAT_EQ(data[0].y, 20.0f);
}

TEST(SlotMapTest, SlotMapViewMultipleComponents) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add(Position{5.0f, 6.0f}, Velocity{7.0f, 8.0f});
    ASSERT_TRUE(sm.isValid(h));
    auto posView = sm.view<Position>();
    auto velView = sm.view<Velocity>();
    Position& pos = posView[h];
    Velocity& vel = velView[h];
    EXPECT_FLOAT_EQ(pos.x, 5.0f);
    EXPECT_FLOAT_EQ(pos.y, 6.0f);
    EXPECT_FLOAT_EQ(vel.dx, 7.0f);
    EXPECT_FLOAT_EQ(vel.dy, 8.0f);
    // Const version
    const auto& csm = sm;
    auto cposView = csm.view<Position>();
    auto cvelView = csm.view<Velocity>();
    const Position& cpos = cposView[h];
    const Velocity& cvel = cvelView[h];
    EXPECT_FLOAT_EQ(cpos.x, 5.0f);
    EXPECT_FLOAT_EQ(cpos.y, 6.0f);
    EXPECT_FLOAT_EQ(cvel.dx, 7.0f);
    EXPECT_FLOAT_EQ(cvel.dy, 8.0f);
    // Raw data
    const auto& posData = posView.rawData();
    const auto& velData = velView.rawData();
    ASSERT_EQ(posData.size(), 1u);
    ASSERT_EQ(velData.size(), 1u);
    EXPECT_FLOAT_EQ(posData[0].x, 5.0f);
    EXPECT_FLOAT_EQ(velData[0].dx, 7.0f);
}

TEST(SlotMapTest, OperatorIndexFirstComponentMultipleTypes) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add(Position{5.0f, 6.0f}, Velocity{7.0f, 8.0f});
    ASSERT_TRUE(sm.isValid(h));
    // operator[] returns first component (Position)
    Position& pos = sm[h];
    EXPECT_FLOAT_EQ(pos.x, 5.0f);
    EXPECT_FLOAT_EQ(pos.y, 6.0f);
    // Const version
    const auto& csm = sm;
    const Position& cpos = csm[h];
    EXPECT_FLOAT_EQ(cpos.x, 5.0f);
    EXPECT_FLOAT_EQ(cpos.y, 6.0f);
    // Access second component via get
    auto* vel = sm.get<Velocity>(h);
    ASSERT_NE(vel, nullptr);
    EXPECT_FLOAT_EQ(vel->dx, 7.0f);
    EXPECT_FLOAT_EQ(vel->dy, 8.0f);
}

TEST(SlotMapTest, KeyValueIterationSingleComponent) {
    SlotMap<Position> sm;
    auto h1 = sm.add(Position{1.0f, 2.0f});
    auto h2 = sm.add(Position{3.0f, 4.0f});
    std::vector<Position> found;
    std::vector<SlotMap<Position>::Key> keys;
    for (const auto& [key, pos] : sm) {
        found.push_back(pos);
        keys.push_back(key);
        EXPECT_TRUE(sm.isValid(key));
    }
    ASSERT_EQ(found.size(), 2u);
    EXPECT_FLOAT_EQ(found[0].x, 1.0f);
    EXPECT_FLOAT_EQ(found[1].y, 4.0f);
    EXPECT_TRUE((keys[0] == h1 || keys[0] == h2));
    EXPECT_TRUE((keys[1] == h1 || keys[1] == h2));
}

TEST(SlotMapTest, KeyValueIterationMultipleComponents) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1,2}, Velocity{3,4});
    auto h2 = sm.add(Position{5,6}, Velocity{7,8});
    std::vector<Position> found;
    std::vector<SlotMap<Position, Velocity>::Key> keys;
    for (const auto& [key, pos] : sm) {
        found.push_back(pos);
        keys.push_back(key);
        EXPECT_TRUE(sm.isValid(key));
    }
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].x + found[1].x, 6);
    EXPECT_TRUE((keys[0] == h1 || keys[0] == h2));
    EXPECT_TRUE((keys[1] == h1 || keys[1] == h2));
}

TEST(SlotMapTest, KeyValueIterationSlotMapView) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{10,20}, Velocity{30,40});
    auto h2 = sm.add(Position{50,60}, Velocity{70,80});
    auto view = sm.view<Velocity>();
    std::vector<Velocity> found;
    std::vector<SlotMap<Position, Velocity>::Key> keys;
    for (const auto& [key, vel] : view) {
        found.push_back(vel);
        keys.push_back(key);
        EXPECT_TRUE(sm.isValid(key));
    }
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].dx + found[1].dx, 100);
    EXPECT_TRUE((keys[0] == h1 || keys[0] == h2));
    EXPECT_TRUE((keys[1] == h1 || keys[1] == h2));
}

TEST(SlotMapTest, RawDataIterationStillWorks) {
    SlotMap<Position> sm;
    sm.add(Position{1,2});
    sm.add(Position{3,4});
    std::vector<float> xs;
    for (auto it = sm.rawBegin(); it != sm.rawEnd(); ++it) {
        xs.push_back(it->x);
    }
    ASSERT_EQ(xs.size(), 2u);
    EXPECT_EQ(xs[0], 1);
    EXPECT_EQ(xs[1], 3);
}

TEST(SlotMapTest, ConstKeyValueIterationSlotMap) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{1,2}, Velocity{3,4});
    auto h2 = sm.add(Position{5,6}, Velocity{7,8});
    const auto& csm = sm;
    std::vector<Position> found;
    std::vector<SlotMap<Position, Velocity>::Key> keys;
    for (const auto& [key, pos] : csm) {
        found.push_back(pos);
        keys.push_back(key);
        EXPECT_TRUE(csm.isValid(key));
    }
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].x + found[1].x, 6);
    EXPECT_TRUE((keys[0] == h1 || keys[0] == h2));
    EXPECT_TRUE((keys[1] == h1 || keys[1] == h2));
}

TEST(SlotMapTest, ConstKeyValueIterationSlotMapView) {
    SlotMap<Position, Velocity> sm;
    auto h1 = sm.add(Position{10,20}, Velocity{30,40});
    auto h2 = sm.add(Position{50,60}, Velocity{70,80});
    const auto& csm = sm;
    auto cview = csm.view<Velocity>();
    std::vector<Velocity> found;
    std::vector<SlotMap<Position, Velocity>::Key> keys;
    for (const auto& [key, vel] : cview) {
        found.push_back(vel);
        keys.push_back(key);
        EXPECT_TRUE(csm.isValid(key));
    }
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].dx + found[1].dx, 100);
    EXPECT_TRUE((keys[0] == h1 || keys[0] == h2));
    EXPECT_TRUE((keys[1] == h1 || keys[1] == h2));
}

TEST(SlotMapTest, AddDefaultConstructAllComponents) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add();
    ASSERT_TRUE(sm.isValid(h));
    auto* pos = sm.get<Position>(h);
    auto* vel = sm.get<Velocity>(h);
    ASSERT_NE(pos, nullptr);
    ASSERT_NE(vel, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 0.0f);
    EXPECT_FLOAT_EQ(pos->y, 0.0f);
    EXPECT_FLOAT_EQ(vel->dx, 0.0f);
    EXPECT_FLOAT_EQ(vel->dy, 0.0f);
}

TEST(SlotMapTest, AddDefaultConstructMissingComponents) {
    SlotMap<Position, Velocity> sm;
    auto h = sm.add(Position{1.5f, 2.5f});
    ASSERT_TRUE(sm.isValid(h));
    auto* pos = sm.get<Position>(h);
    auto* vel = sm.get<Velocity>(h);
    ASSERT_NE(pos, nullptr);
    ASSERT_NE(vel, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.5f);
    EXPECT_FLOAT_EQ(pos->y, 2.5f);
    EXPECT_FLOAT_EQ(vel->dx, 0.0f);
    EXPECT_FLOAT_EQ(vel->dy, 0.0f);
}
