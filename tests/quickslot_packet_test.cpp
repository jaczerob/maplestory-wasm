#include <doctest/doctest.h>

#include "client/Net/Handlers/Helpers/QuickslotParser.h"
#include "client/Net/Packets/PlayerPackets.h"

#include <array>
#include <cstdint>
#include <vector>

namespace
{
    void append_int32(std::vector<int8_t>& bytes, int32_t value)
    {
        const uint32_t raw = static_cast<uint32_t>(value);
        for (int shift = 0; shift < 32; shift += 8)
        {
            bytes.push_back(static_cast<int8_t>(raw >> shift));
        }
    }
}

TEST_CASE("Quickslot change packet sends eight int32 key codes")
{
    const jrc::Keyboard::Quickslots keys{ 42, 82, 71, 73, 29, 83, 79, 255 };
    const jrc::ChangeQuickslotPacket packet(keys);
    const auto& bytes = packet.data();

    REQUIRE(bytes.size() == 2 + keys.size() * 4);
    CHECK(static_cast<uint8_t>(bytes[0]) == 0xB7);
    CHECK(static_cast<uint8_t>(bytes[1]) == 0);
    for (size_t i = 0; i < keys.size(); ++i)
    {
        CHECK(static_cast<uint8_t>(bytes[2 + i * 4]) == keys[i]);
        CHECK(static_cast<uint8_t>(bytes[3 + i * 4]) == 0);
        CHECK(static_cast<uint8_t>(bytes[4 + i * 4]) == 0);
        CHECK(static_cast<uint8_t>(bytes[5 + i * 4]) == 0);
    }
}

TEST_CASE("Quickslot init restores native defaults or decodes all custom keys")
{
    std::array<int8_t, 1> defaults{ 0 };
    jrc::InPacket default_packet(defaults.data(), defaults.size());
    jrc::Keyboard::Quickslots keys{};
    jrc::parse_quickslot_init(default_packet, keys);
    CHECK(keys == jrc::Keyboard::DEFAULT_QUICKSLOTS);

    std::vector<int8_t> custom{ 1 };
    const std::array<int32_t, 8> wire_keys{ 42, -128, -1, 0, 82, 71, 73, 255 };
    for (int32_t key : wire_keys)
    {
        append_int32(custom, key);
    }
    jrc::InPacket custom_packet(custom.data(), custom.size());
    jrc::parse_quickslot_init(custom_packet, keys);
    CHECK(keys == (jrc::Keyboard::Quickslots{ 42, 128, 255, 0, 82, 71, 73, 255 }));
}

TEST_CASE("Malformed quickslot init leaves the existing layout unchanged")
{
    const jrc::Keyboard::Quickslots existing{ 1, 2, 3, 4, 5, 6, 7, 8 };
    const std::array<int8_t, 2> trailing{ 0, 0 };
    jrc::Keyboard::Quickslots keys = existing;
    jrc::InPacket trailing_packet(trailing.data(), trailing.size());
    CHECK_THROWS_AS(jrc::parse_quickslot_init(trailing_packet, keys), jrc::PacketError);
    CHECK(keys == existing);

    std::vector<int8_t> invalid{ 1 };
    for (size_t i = 0; i < 8; ++i)
    {
        append_int32(invalid, i == 7 ? 256 : static_cast<int32_t>(i));
    }
    jrc::InPacket invalid_packet(invalid.data(), invalid.size());
    CHECK_THROWS_AS(jrc::parse_quickslot_init(invalid_packet, keys), jrc::PacketError);
    CHECK(keys == existing);

    invalid[1] = static_cast<int8_t>(0x7F);
    invalid[2] = static_cast<int8_t>(0xFF);
    invalid[3] = static_cast<int8_t>(0xFF);
    invalid[4] = static_cast<int8_t>(0xFF); // -129 is outside the signed-byte range.
    jrc::InPacket signed_overflow_packet(invalid.data(), invalid.size());
    CHECK_THROWS_AS(jrc::parse_quickslot_init(signed_overflow_packet, keys), jrc::PacketError);
    CHECK(keys == existing);

    const std::array<int8_t, 5> truncated{ 1, 0, 0, 0, 0 };
    jrc::InPacket truncated_packet(truncated.data(), truncated.size());
    CHECK_THROWS_AS(jrc::parse_quickslot_init(truncated_packet, keys), jrc::PacketError);
    CHECK(keys == existing);
}
