//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "MapReactors.h"

#include "Reactor.h"

#include "../../Net/Packets/AttackAndSkillPackets.h"

namespace jrc
{
    void MapReactors::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        reactors.draw(layer, viewx, viewy, alpha);
    }

    void MapReactors::update(const Physics& physics)
    {
        for (; !spawns.empty(); spawns.pop())
        {
            const ReactorSpawn& spawn = spawns.front();

            int32_t oid = spawn.get_oid();
            reactors.remove(oid);
            reactors.add(spawn.instantiate(physics));
        }

        reactors.update(physics);
    }

    void MapReactors::spawn(ReactorSpawn&& spawn)
    {
        spawns.emplace(
            std::move(spawn)
        );
    }

    void MapReactors::remove(int32_t oid, int8_t state, Point<int16_t> position)
    {
        if (Optional<Reactor> reactor = reactors.get(oid))
        {
            reactor->destroy(state, position);
        }
    }

    void MapReactors::hit(int32_t oid, int8_t state, Point<int16_t> position)
    {
        if (Optional<Reactor> reactor = reactors.get(oid))
        {
            reactor->hit(state, position);
        }
    }

    void MapReactors::send_attack(const Attack& attack)
    {
        if (attack.type != Attack::CLOSE)
            return;

        for (const auto& object : reactors)
        {
            const Reactor* reactor = static_cast<const Reactor*>(object.second.get());
            if (reactor && reactor->is_attackable() && attack.hits_reactor(reactor->get_bounds()))
            {
                DamageReactorPacket(object.first, attack.origin.x(), attack.stance, attack.skill).dispatch();
            }
        }
    }

    void MapReactors::clear()
    {
        reactors.clear();
    }
}
