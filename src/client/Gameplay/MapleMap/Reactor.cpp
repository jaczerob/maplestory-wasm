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
#include "Reactor.h"

#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    Reactor::Reactor(int32_t o, int32_t r, int8_t s, Point<int16_t> p)
        : MapObject(o, p), rid(r), state(s) {

        std::string strid = string_format::extend_id(rid, 7);
        source = nl::nx::reactor[strid + ".img"];

        nl::node initial = source[std::to_string(state)];
        normal = initial;
        destroying = initial["event"].size() == 0;
    }

    int8_t Reactor::update(const Physics& physics)
    {
        if (!active)
            return phobj.fhlayer;

        if (playing_hit)
        {
            if (hitanimation.update())
            {
                playing_hit = false;
                normal = source[std::to_string(state)];
                normal.reset();
            }
        }
        else if (destroying)
        {
            if (normal.update())
                deactivate();
        }
        else
        {
            normal.update();
        }
        return MapObject::update(physics);
    }

    void Reactor::draw(double viewx, double viewy, float alpha) const
    {
        Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);
        const Animation& animation = playing_hit ? hitanimation : normal;
        Point<int16_t> shift = { 0, animation.get_dimensions().y() / 2 };
        animation.draw(absp - shift, alpha);
    }

    bool Reactor::is_attackable() const
    {
        return active && !destroying;
    }

    Rectangle<int16_t> Reactor::get_bounds() const
    {
        Point<int16_t> origin = normal.get_origin();
        Point<int16_t> dimensions = normal.get_dimensions();
        Point<int16_t> position = get_position();
        int16_t top = position.y() - dimensions.y() / 2 - origin.y();
        return {
            position.x() - origin.x(), position.x() - origin.x() + dimensions.x(),
            top, top + dimensions.y()
        };
    }

    void Reactor::hit(int8_t newstate, Point<int16_t> position)
    {
        nl::node hit = source[std::to_string(state)]["hit"];
        nl::node next = source[std::to_string(newstate)];
        set_position(position);
        state = newstate;
        destroying = next["event"].size() == 0;
        if (hit.size() > 0)
        {
            hitanimation = hit;
            hitanimation.reset();
            playing_hit = true;
        }
        else
        {
            normal = next;
            normal.reset();
        }
    }

    void Reactor::destroy(int8_t newstate, Point<int16_t> position)
    {
        state = newstate;
        set_position(position);
        destroying = true;
        if (!playing_hit)
        {
            normal = source[std::to_string(state)];
            normal.reset();
        }
    }
}
