#pragma once

#include "../../../IO/Keyboard.h"
#include "../../InPacket.h"

namespace jrc
{
    void parse_quickslot_init(InPacket& recv, Keyboard::Quickslots& keys);
}
