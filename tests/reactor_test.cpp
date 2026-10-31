#include <doctest/doctest.h>

#include "client/Gameplay/Combat/Attack.h"
#include "client/Net/Packets/AttackAndSkillPackets.h"

#include <vector>

TEST_CASE("Only close attacks intersecting reactor bounds send hits")
{
    jrc::Attack attack;
    attack.type = jrc::Attack::CLOSE;
    attack.origin = {100, 50};
    attack.range = {10, 40, -15, 15};
    attack.toleft = true;

    CHECK(attack.hits_reactor({138, 142, 45, 55}));
    CHECK_FALSE(attack.hits_reactor({145, 150, 45, 55}));

    attack.toleft = false;
    CHECK(attack.hits_reactor({60, 65, 45, 55}));
    CHECK_FALSE(attack.hits_reactor({50, 55, 45, 55}));

    attack.type = jrc::Attack::RANGED;
    CHECK_FALSE(attack.hits_reactor({138, 142, 45, 55}));
}

TEST_CASE("Reactor hit requests match the server field order")
{
    jrc::DamageReactorPacket packet(0x01020304, -25, 0x1234, 0x11223344);
    const std::vector<int8_t> expected{
        static_cast<int8_t>(0xCD), 0,
        4, 3, 2, 1,
        static_cast<int8_t>(0xE7), static_cast<int8_t>(0xFF), static_cast<int8_t>(0xFF), static_cast<int8_t>(0xFF),
        0x34, 0x12,
        0, 0, 0, 0,
        0x44, 0x33, 0x22, 0x11
    };
    CHECK(packet.data() == expected);
}
