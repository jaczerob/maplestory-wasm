#include "support/packet_fixture.h"

#include "client/Character/Inventory/EquipQuality.h"
#include "client/Graphics/Texture.h"
#include "client/Net/Handlers/Helpers/MTSParser.h"
#include "client/Net/Packets/MTSPackets.h"

namespace jrc
{
    Texture::~Texture() {}

    namespace EquipQuality
    {
        Id check_quality(int32_t, bool, const EnumMap<Equipstat::Id, uint16_t>&)
        {
            return WHITE;
        }
    }
}

namespace
{
    using namespace jrc;
    using test_support::PacketFixture;
    using test_support::expect_packet;

    void write_common_listing(PacketFixture& packet, int32_t item_id, int32_t quantity, int32_t id)
    {
        packet.write_byte(2);
        packet.write_int(item_id);
        packet.write_byte(0);
        packet.write_long(0);
        packet.write_short(static_cast<int16_t>(quantity));
        packet.write_string("Seller");
        packet.write_short(0);
        packet.write_int(id);
        packet.write_int(115);
        packet.write_int(1500);
        packet.write_int(0);
        packet.write_long(0);
        packet.write_string("Seller");
        packet.write_string("Seller");
        packet.skip(28);
    }

    void write_equip_listing(PacketFixture& packet, int32_t id)
    {
        packet.write_byte(1);
        packet.write_int(1000000);
        packet.write_byte(0);
        packet.write_long(0);
        packet.write_byte(0);
        packet.write_byte(0);
        for (int i = 0; i < Equipstat::LENGTH; ++i)
            packet.write_short(0);
        packet.write_string("");
        packet.write_short(0);
        packet.write_byte(0);
        packet.write_byte(0);
        packet.write_short(0);
        packet.write_short(0);
        packet.write_int(0);
        packet.write_long(0);
        packet.skip(12);
        packet.write_int(id);
        packet.write_int(115);
        packet.write_int(1500);
        packet.write_int(0);
        packet.write_long(0);
        packet.write_string("Seller");
        packet.write_string("Seller");
        packet.skip(28);
    }
}

TEST_CASE("MTS browse requests encode the requested category and page")
{
    using namespace jrc;
    expect_packet(MTSBrowsePacket(1, 4, 3),
        {0xFD, 0, 5, 1, 0, 0, 0, 4, 0, 0, 0, 3, 0, 0, 0});
}

TEST_CASE("MTS entry tail reads the character terminator and account")
{
    using namespace jrc;
    PacketFixture packet;
    packet.write_short(0);
    packet.write_string("account");
    packet.skip(28);
    auto input = packet.input();
    CHECK(MTSParser::entry_account(input) == "account");
    CHECK_FALSE(input.available());

    PacketFixture truncated;
    truncated.write_short(0);
    truncated.write_string("account");
    auto short_input = truncated.input();
    CHECK_THROWS_AS(MTSParser::entry_account(short_input), PacketError);
}

TEST_CASE("MTS listings parse short ETC quantity and equipment")
{
    using namespace jrc;
    PacketFixture packet;
    packet.write_int(32);
    packet.write_int(2);
    packet.write_int(1);
    packet.write_int(4);
    packet.write_int(0);
    packet.skip(2);
    write_common_listing(packet, 4000000, 7, 41);
    write_equip_listing(packet, 42);
    packet.write_byte(1);

    auto input = packet.input();
    const auto page = MTSParser::page(input);
    REQUIRE(page.listings.size() == 2);
    CHECK(page.total == 32);
    CHECK(page.category == 4);
    CHECK(page.listings[0].item_id == 4000000);
    CHECK(page.listings[0].quantity == 7);
    CHECK(page.listings[0].id == 41);
    CHECK(page.listings[0].fee == 115);
    CHECK(page.listings[0].price == 1500);
    CHECK(page.listings[1].item_id == 1000000);
    CHECK(page.listings[1].quantity == 1);
    CHECK_FALSE(input.available());
}

TEST_CASE("MTS page parser rejects invalid counts and category")
{
    using namespace jrc;
    PacketFixture too_many;
    too_many.write_int(16);
    too_many.write_int(17);
    too_many.write_int(1);
    too_many.write_int(0);
    too_many.write_int(0);
    auto count_input = too_many.input();
    CHECK_THROWS_AS(MTSParser::page(count_input), PacketError);

    PacketFixture bad_category;
    bad_category.write_int(0);
    bad_category.write_int(0);
    bad_category.write_int(1);
    bad_category.write_int(6);
    bad_category.write_int(0);
    auto category_input = bad_category.input();
    CHECK_THROWS_AS(MTSParser::page(category_input), PacketError);
}
