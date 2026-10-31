#include "support/assets.h"

#include <doctest/doctest.h>

#include "client/Gameplay/Physics/Physics.h"
#include "nlnx/nx.hpp"

#include <cstdint>
#include <string>

namespace
{
    nl::node find_foothold(const nl::node& footholds, uint16_t id)
    {
        for (const nl::node& layer : footholds)
        {
            for (const nl::node& group : layer)
            {
                for (const nl::node& foothold : group)
                {
                    if (foothold.name() == std::to_string(id))
                    {
                        return foothold;
                    }
                }
            }
        }
        return {};
    }
}

TEST_CASE("Map 50000 rejoin uses the foothold under a grounded snail")
{
    test_support::NxFile map_file("Map.nx", nl::nx::map);
    const nl::node map = nl::nx::map["Map"]["Map0"]["000050000.img"];
    REQUIRE(map);

    const nl::node fh18 = find_foothold(map["foothold"], 18);
    REQUIRE(fh18);

    const jrc::Physics physics(map["foothold"]);

    jrc::PhysicsObject current_contact;
    current_contact.set_x(830);
    current_contact.set_y(395);
    current_contact.fhid = 18;
    current_contact.set_flag(jrc::PhysicsObject::TURNATEDGES);
    for (int tick = 0; tick < 100; ++tick)
    {
        physics.move_object(current_contact);
    }
    CHECK(current_contact.get_y() == 395);
    CHECK(current_contact.fhid == 18);
    CHECK(current_contact.onground);
}

TEST_CASE("Airborne mobs are not grounded by rejoin contact checks")
{
    test_support::NxFile map_file("Map.nx", nl::nx::map);
    const nl::node map = nl::nx::map["Map"]["Map0"]["000050000.img"];
    REQUIRE(map);

    const jrc::Physics physics(map["foothold"]);
    jrc::PhysicsObject flying;
    flying.type = jrc::PhysicsObject::FLYING;
    flying.onground = false;
    flying.set_x(830);
    flying.set_y(380);
    flying.fhid = 2;
    for (int tick = 0; tick < 20; ++tick)
    {
        physics.move_object(flying);
    }
    CHECK(flying.get_y() == 380);
    CHECK_FALSE(flying.onground);
}
