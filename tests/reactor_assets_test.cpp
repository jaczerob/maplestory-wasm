#include "support/assets.h"
#include "support/render_capture.h"

#include <doctest/doctest.h>

#include "client/Gameplay/MapleMap/Reactor.h"
#include "client/Gameplay/Physics/Physics.h"
#include "nlnx/nx.hpp"

#include <set>

namespace jrc
{
    Footholdtree::Footholdtree() = default;
    Physics::Physics() = default;
    void Physics::move_object(PhysicsObject&) const {}
}

namespace
{
    uint64_t draw_reactor(jrc::Reactor& reactor)
    {
        test_support::draws.clear();
        reactor.draw(0, 0, 1);
        REQUIRE(test_support::draws.size() == 1);
        return test_support::draws.front().bitmap.id();
    }
}

TEST_CASE("Reactor hits animate through server states before terminal removal")
{
    test_support::NxFile assets("Reactor.nx", nl::nx::reactor);
    const jrc::Point<int16_t> position(180, 200);
    jrc::Reactor reactor(1, 2001, 0, position);
    jrc::Physics physics;

    const uint64_t initial = draw_reactor(reactor);
    const auto bounds = reactor.get_bounds();
    const auto& drawn = test_support::draws.front().bounds;
    CHECK(bounds.l() == drawn.l());
    CHECK(bounds.r() == drawn.r());
    CHECK(bounds.t() == drawn.t());
    CHECK(bounds.b() == drawn.b());
    CHECK(reactor.is_attackable());

    nl::node templateData = assets.root()["0002001.img"];
    for (int8_t state = 1; state <= 3; ++state)
    {
        std::set<uint64_t> idleframes;
        for (nl::node frame : templateData[std::to_string(state)])
        {
            if (frame.data_type() == nl::node::type::bitmap)
                idleframes.insert(frame.get_bitmap().id());
        }
        REQUIRE_FALSE(idleframes.empty());

        reactor.hit(state, position);
        const uint64_t hit = draw_reactor(reactor);
        CHECK(hit != initial);

        for (int tick = 0; tick < 1000 && !idleframes.count(draw_reactor(reactor)); ++tick)
            reactor.update(physics);

        CHECK(idleframes.count(draw_reactor(reactor)) > 0);
        CHECK(reactor.is_active());
        CHECK(reactor.is_attackable());
    }

    reactor.hit(4, position);
    CHECK_FALSE(reactor.is_attackable());
    CHECK(reactor.is_active());
    CHECK(draw_reactor(reactor) != 0);

    for (int tick = 0; tick < 2000 && reactor.is_active(); ++tick)
        reactor.update(physics);

    CHECK_FALSE(reactor.is_active());

    jrc::Reactor terminal(2, 2001, 4, position);
    CHECK_FALSE(terminal.is_attackable());
    for (int tick = 0; tick < 2000 && terminal.is_active(); ++tick)
        terminal.update(physics);
    CHECK_FALSE(terminal.is_active());
}
