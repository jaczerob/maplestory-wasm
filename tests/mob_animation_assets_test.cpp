#include "support/assets.h"

#include <doctest/doctest.h>

#include "client/Graphics/Animation.h"

TEST_CASE("Mob 3000001 skill1 retains all nine 100ms animation frames")
{
    test_support::NxFile mob("Mob.nx");
    const auto mob_node = mob.root()["3000001.img"];
    const auto source = mob_node["skill1"];

    const jrc::Animation animation(source);
    for (int16_t frame = 0; frame < 9; ++frame)
        CHECK(animation.get_delay(frame) == 100);
    CHECK(animation.get_delay(9) == 0);
}
