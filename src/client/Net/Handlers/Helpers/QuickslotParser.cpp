#include "QuickslotParser.h"

namespace jrc
{
    void parse_quickslot_init(InPacket& recv, Keyboard::Quickslots& keys)
    {
        const int8_t custom = recv.read_byte();
        if (custom == 0)
        {
            if (recv.length() != 0)
            {
                throw PacketError("Unexpected quickslot default payload");
            }
            keys = Keyboard::DEFAULT_QUICKSLOTS;
            return;
        }
        if (custom != 1 || recv.length() != keys.size() * sizeof(int32_t))
        {
            throw PacketError("Invalid quickslot layout payload");
        }

        Keyboard::Quickslots parsed;
        for (uint8_t& key : parsed)
        {
            const int32_t value = recv.read_int();
            if (value < -128 || value > 255)
            {
                throw PacketError("Quickslot key is outside byte range");
            }
            key = static_cast<uint8_t>(value);
        }
        keys = parsed;
    }
}
