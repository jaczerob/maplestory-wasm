#pragma once

#include "../../InPacket.h"

#include <string>
#include <vector>

namespace jrc
{
    struct MTSListing
    {
        int32_t id = 0;
        int32_t item_id = 0;
        int32_t quantity = 0;
        int32_t fee = 0;
        int32_t price = 0;
        std::string seller;
    };

    struct MTSPage
    {
        int32_t total = 0;
        int32_t tab = 1;
        int32_t category = 0;
        int32_t page = 0;
        std::vector<MTSListing> listings;
    };

    namespace MTSParser
    {
        std::string entry_account(InPacket& recv);
        std::vector<MTSListing> listings(InPacket& recv);
        MTSPage page(InPacket& recv);
    }
}
