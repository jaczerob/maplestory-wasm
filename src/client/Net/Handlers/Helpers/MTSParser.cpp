#include "MTSParser.h"
#include "ItemParser.h"

#include <limits>

namespace jrc::MTSParser
{
    std::string entry_account(InPacket& recv)
    {
        recv.read_short(); // Character-information terminator.
        std::string account = recv.read_string();
        recv.skip(28); // MTS setup constants.
        if (recv.available())
            throw PacketError("Invalid MTS entry packet tail");
        return account;
    }

    static std::vector<MTSListing> read_listings(InPacket& recv, int32_t count)
    {
        if (count < 0 || count > 1000 || static_cast<size_t>(count) > recv.length() / 68)
            throw PacketError("Invalid Trade listing count");

        std::vector<MTSListing> result;
        for (int32_t i = 0; i < count; ++i)
        {
            auto peek = recv;
            const int8_t kind = peek.read_byte();
            const int32_t item_id = peek.read_int();
            if ((kind != 1 && kind != 2) || item_id < 1000000 || item_id >= 5000000 ||
                (kind == 1) != (item_id / 1000000 == 1))
                throw PacketError("Invalid Trade item");

            Inventory inventory;
            // MTS item records use a short quantity even for ETC listings.
            const auto type = kind == 1 ? InventoryType::EQUIP : InventoryType::USE;
            ItemParser::parse_item(recv, type, 1, inventory);
            MTSListing listing;
            listing.item_id = item_id;
            listing.quantity = kind == 1 ? 1 : inventory.get_item_count(type, 1);
            listing.id = recv.read_int();
            listing.fee = recv.read_int();
            listing.price = recv.read_int();
            recv.skip(12); // Reserved field and sale expiration.
            listing.seller = recv.read_string();
            recv.read_string();
            recv.skip(28);
            if (listing.id <= 0 || listing.quantity <= 0 || listing.fee < 0 || listing.price <= 0 ||
                listing.price > std::numeric_limits<int32_t>::max() - listing.fee)
                throw PacketError("Invalid Trade listing");
            result.push_back(std::move(listing));
        }
        return result;
    }

    std::vector<MTSListing> listings(InPacket& recv)
    {
        const auto count = recv.read_int();
        return read_listings(recv, count);
    }

    MTSPage page(InPacket& recv)
    {
        MTSPage result;
        result.total = recv.read_int();
        const int32_t count = recv.read_int();
        result.tab = recv.read_int();
        result.category = recv.read_int();
        result.page = recv.read_int();
        if (result.total < 0 || result.tab < 1 || result.tab > 5 ||
            result.category < 0 || result.category > 5 || result.page < 0 || result.page > 625 ||
            count < 0 || count > 16)
            throw PacketError("Invalid Trade page");
        recv.skip(2);
        result.listings = read_listings(recv, count);
        if (recv.read_byte() != 1 || recv.available())
            throw PacketError("Invalid Trade page terminator");
        return result;
    }
}
