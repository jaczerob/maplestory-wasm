#include "support/assets.h"
#include "support/render_capture.h"
#include <doctest/doctest.h>

#include "client/IO/UI.h"
#include "client/IO/UITypes/UIQuickSlot.h"
#include "client/Graphics/GraphicsGL.h"
#include "client/Graphics/Geometry.h"
#include "client/Audio/Audio.h"
#include "client/Net/OutPacket.h"
#include "nlnx/nx.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>

namespace
{
    std::vector<std::vector<int8_t>> packets;
    std::vector<std::string> rendered_text;
    uint64_t press_id = 0;

    class ProbeQuickSlot : public jrc::UIQuickSlot
    {
    public:
        using UIQuickSlot::UIQuickSlot;

        void select_slot(int slot)
        {
            const int x = 64 + slot % 4 * 35 + 15;
            const int y = 113 + slot / 4 * 34 + 14;
            ++press_id;
            send_window_cursor(true, position + jrc::Point<int16_t>(x, y));
        }

        void press_button(uint16_t id) { button_pressed(id); }
    };
}

namespace jrc
{
    UI::UI() : cursor_press_id(0), enabled(true), quitted(false) {}
    uint64_t UI::get_cursor_press_id() const { return press_id; }
    Keyboard& UI::get_keyboard() { return keyboard; }
    GraphicsGL::GraphicsGL() : locked(false) {}
    Text::Layout GraphicsGL::createlayout(const std::string&, Text::Font, Text::Alignment, int16_t, bool) { return {}; }
    void GraphicsGL::drawtext(const DrawArgument&, const std::string& text, const Text::Layout&, Text::Font,
        Text::Color, Text::Background, const Range<int16_t>*) { rendered_text.push_back(text); }
    Cursor::Cursor() : state(IDLE), hide_counter(0) {}
    ScrollingNotice::ScrollingNotice() : active(false) {}
    Sound::Sound(Name) : id(0) {}
    void Sound::play() const {}
    void OutPacket::dispatch() { packets.push_back(data()); }
    ColorBox::ColorBox() : width(0), height(0), color(BLACK), opacity(0) {}
    ColorBox::ColorBox(int16_t width, int16_t height, Geometry::Color color, float opacity)
        : width(width), height(height), color(color), opacity(opacity) {}
    void ColorBox::draw(const DrawArgument&) const {}
}

TEST_CASE("quick slot editor captures keys, swaps duplicates, and applies changes")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Keyboard keyboard;
    ProbeQuickSlot editor(keyboard);
    packets.clear();

    test_support::draws.clear();
    editor.draw(0.1f);
    test_support::snapshot(test_support::artifact("quickslot-editor.ppm"), 800, 600);

    CHECK_FALSE(editor.assign_key(GLFW_KEY_Q));
    editor.select_slot(0);
    CHECK(editor.is_capturing());
    CHECK_FALSE(editor.assign_key(GLFW_KEY_LEFT));
    CHECK(editor.is_capturing());
    CHECK(editor.assign_key(GLFW_KEY_Q));
    CHECK_FALSE(editor.is_capturing());

    editor.select_slot(1);
    CHECK(editor.assign_key(GLFW_KEY_Q));
    editor.press_button(2);

    CHECK(keyboard.get_quickslots()[0] == 82);
    CHECK(keyboard.get_quickslots()[1] == 16);
    REQUIRE(packets.size() == 1);
    REQUIRE(packets[0].size() == 34);
    CHECK(static_cast<uint8_t>(packets[0][0]) == 0xB7);
    CHECK(static_cast<uint8_t>(packets[0][2]) == 82);
    CHECK(static_cast<uint8_t>(packets[0][6]) == 16);
}

TEST_CASE("quick slot cancel discards staged keys and default applies only on OK")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Keyboard keyboard;
    Keyboard::Quickslots custom = Keyboard::DEFAULT_QUICKSLOTS;
    custom[0] = 16;
    keyboard.set_quickslots(custom);
    ProbeQuickSlot cancelled(keyboard);
    packets.clear();

    cancelled.select_slot(0);
    REQUIRE(cancelled.assign_key(GLFW_KEY_W));
    cancelled.press_button(1);
    CHECK(keyboard.get_quickslots() == custom);
    CHECK(packets.empty());

    ProbeQuickSlot defaults(keyboard);
    defaults.select_slot(0);
    REQUIRE(defaults.assign_key(GLFW_KEY_W));
    defaults.press_button(0);
    defaults.press_button(2);
    CHECK(keyboard.get_quickslots() == Keyboard::DEFAULT_QUICKSLOTS);
    CHECK(packets.size() == 1);
}

TEST_CASE("quick slot aliases right modifiers and labels Scroll Lock")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Keyboard keyboard;
    ProbeQuickSlot editor(keyboard);
    editor.select_slot(0);
    REQUIRE(editor.assign_key(GLFW_KEY_RIGHT_ALT));
    editor.press_button(2);
    CHECK(keyboard.get_quickslots()[0] == 56);

    Keyboard::Quickslots keys = keyboard.get_quickslots();
    keys[0] = 88;
    keyboard.set_quickslots(keys);
    ProbeQuickSlot scroll_lock(keyboard);
    rendered_text.clear();
    scroll_lock.draw(1.0f);
    CHECK(std::find(rendered_text.begin(), rendered_text.end(), "ScrLk") != rendered_text.end());
    test_support::draws.clear();
    scroll_lock.draw(1.0f);
    test_support::snapshot(test_support::artifact("quickslot-scrolllock.ppm"), 800, 600);
}
