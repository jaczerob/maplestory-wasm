#pragma once

#include "../OutPacket.h"

namespace jrc
{
    class MTSBrowsePacket : public OutPacket
    {
    public:
        MTSBrowsePacket(int32_t tab, int32_t category, int32_t page) : OutPacket(0xFD)
        {
            write_byte(5);
            write_int(tab);
            write_int(category);
            write_int(page);
        }
    };
}
