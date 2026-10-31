#include <doctest/doctest.h>

#include "client/Audio/Audio.h"
#include "client/Graphics/Geometry.h"
#include "client/Graphics/GraphicsGL.h"
#include "client/IO/UI.h"
#include "client/IO/UITypes/UIKeyConfig.h"
#include "client/Net/OutPacket.h"

#include <cstdint>
#include <vector>

namespace
{
    std::vector<std::vector<int8_t>> packets;
}

namespace jrc
{
    GraphicsGL::GraphicsGL() : locked(false) {}
    Text::Layout GraphicsGL::createlayout(const std::string&, Text::Font, Text::Alignment, int16_t, bool) { return {}; }
    void GraphicsGL::drawtext(const DrawArgument&, const std::string&, const Text::Layout&, Text::Font,
        Text::Color, Text::Background, const Range<int16_t>*) {}
    ColorBox::ColorBox() : width(0), height(0), color(BLACK), opacity(0) {}

    Configuration::Configuration() = default;
    Configuration::~Configuration() = default;
    UI::UI() : state(std::make_unique<UIStateNull>()), cursor_press_id(0), enabled(true), quitted(false) {}

    Keyboard& UI::get_keyboard()
    {
        return keyboard;
    }

    uint64_t UI::get_cursor_press_id() const
    {
        return cursor_press_id;
    }

    Cursor::Cursor() : state(IDLE), hide_counter(0) {}
    ScrollingNotice::ScrollingNotice() : active(false) {}
    Sound::Sound(Name) : id(0) {}
    void Sound::play() const {}
    void OutPacket::dispatch() { packets.push_back(data()); }
}

TEST_CASE("Quickslot skill assignment moves one binding and removes duplicates")
{
    using namespace jrc;
    Keyboard& keyboard = UI::get().get_keyboard();
    const Keyboard::Mapping skill{ KeyType::SKILL, 1001000 };
    keyboard.assign(30, skill.type, skill.action);
    keyboard.assign(31, skill.type, skill.action);
    packets.clear();

    UIKeyConfig::assign_quickslot_mapping(32, skill);

    CHECK(keyboard.get_maple_mapping(30) == Keyboard::Mapping{});
    CHECK(keyboard.get_maple_mapping(31) == Keyboard::Mapping{});
    CHECK(keyboard.get_maple_mapping(32) == skill);
    REQUIRE(packets.size() == 1);
    CHECK(packets.front().size() == 37);
}

TEST_CASE("Quickslot assignment keeps modifier aliases bound together")
{
    using namespace jrc;
    Keyboard& keyboard = UI::get().get_keyboard();
    const Keyboard::Mapping item{ KeyType::ITEM, 2000000 };
    packets.clear();

    UIKeyConfig::assign_quickslot_mapping(KeyConfig::LEFT_CONTROL, item);

    CHECK(keyboard.get_maple_mapping(KeyConfig::LEFT_CONTROL) == item);
    CHECK(keyboard.get_maple_mapping(KeyConfig::RIGHT_CONTROL) == item);
    REQUIRE(packets.size() == 1);
}

TEST_CASE("Unsupported quickslot targets and mapping types leave bindings unchanged")
{
    using namespace jrc;
    Keyboard& keyboard = UI::get().get_keyboard();
    const Keyboard::Mapping skill{ KeyType::SKILL, 1001000 };
    const Keyboard::Mapping action{ KeyType::ACTION, KeyAction::ATTACK };
    keyboard.assign(30, skill.type, skill.action);
    packets.clear();

    UIKeyConfig::assign_quickslot_mapping(255, skill);
    UIKeyConfig::assign_quickslot_mapping(30, {});
    UIKeyConfig::assign_quickslot_mapping(30, action);

    CHECK(keyboard.get_maple_mapping(30) == skill);
    CHECK(keyboard.get_maple_mapping(255) == Keyboard::Mapping{});
    CHECK(packets.empty());
}
