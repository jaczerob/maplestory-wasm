#include "support/assets.h"
#include "support/render_capture.h"
#include <doctest/doctest.h>
#include "client/IO/UI.h"
#include "client/IO/UITypes/UIHudMenus.h"
#include "client/Graphics/GraphicsGL.h"
#include "client/Audio/Audio.h"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <vector>

namespace
{
    using namespace jrc;
    std::vector<KeyAction::Id> actions;
    uint64_t press_id = 0;

    class ProbeMainMenu : public UIMainMenu
    {
    public:
        explicit ProbeMainMenu(Point<int16_t> anchor = {}) : UIMainMenu(anchor) { set_type(TYPE); }

        Point<int16_t> button_center(uint16_t id) const
        {
            const auto bounds = buttons.at(id)->bounds(position);
            return { static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                     static_cast<int16_t>((bounds.t() + bounds.b()) / 2) };
        }

        Point<int16_t> empty_panel_point() const
        {
            return position + Point<int16_t>(dimension.x() - 3, dimension.y() - 3);
        }

        Point<int16_t> location() const { return position; }
        Point<int16_t> size() const { return dimension; }
    };

    class ProbeSystemMenu : public UISystemMenu
    {
    public:
        explicit ProbeSystemMenu(Point<int16_t> anchor) : UISystemMenu(anchor) {}
        Point<int16_t> location() const { return position; }
        Point<int16_t> button_center(uint16_t id) const
        {
            const auto bounds = buttons.at(id)->bounds(position);
            return { static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                     static_cast<int16_t>((bounds.t() + bounds.b()) / 2) };
        }
    };

    const test_support::Draw* find_draw(uint32_t id)
    {
        auto found = std::find_if(test_support::draws.begin(), test_support::draws.end(),
            [id](const auto& draw) { return draw.bitmap.id() == id; });
        return found == test_support::draws.end() ? nullptr : &*found;
    }
}

namespace jrc
{
    GraphicsGL::GraphicsGL() : locked(false) {}
    Text::Layout GraphicsGL::createlayout(const std::string&, Text::Font, Text::Alignment, int16_t, bool) { return {}; }
    void GraphicsGL::drawtext(const DrawArgument&, const std::string&, const Text::Layout&, Text::Font,
        Text::Color, Text::Background, const Range<int16_t>*) {}
    ColorBox::ColorBox() : width(0), height(0), color(BLACK), opacity(0) {}
    UI::UI() : cursor_press_id(0), enabled(true), quitted(false) {}
    void UI::send_menu(KeyAction::Id action) { actions.push_back(action); }
    void UI::quit() { quitted = true; }
    uint64_t UI::get_cursor_press_id() const { return press_id; }
    Keyboard::Keyboard() {}
    Cursor::Cursor() : state(IDLE), hide_counter(0) {}
    ScrollingNotice::ScrollingNotice() : active(false) {}
    Sound::Sound(Name) : id(0) {}
    void Sound::play() const {}
}

TEST_CASE("HUD menus use native popup artwork at stable opacity and dock anchors")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Constants::set_viewsize(800, 600);
    actions.clear();

    ProbeMainMenu menu({712, 554});
    ProbeSystemMenu system({769, 554});
    const auto main_src = nl::nx::ui["StatusBar2.img"]["mainBar"]["Menu"];
    const auto system_src = nl::nx::ui["StatusBar2.img"]["mainBar"]["System"];
    const auto main_top_id = main_src["backgrnd"]["0"].get_bitmap().id();
    const auto main_tile_id = main_src["backgrnd"]["1"].get_bitmap().id();
    const auto main_bottom_id = main_src["backgrnd"]["2"].get_bitmap().id();
    const auto system_top_id = system_src["backgrnd"]["0"].get_bitmap().id();
    const auto system_tile_id = system_src["backgrnd"]["1"].get_bitmap().id();
    const auto system_bottom_id = system_src["backgrnd"]["2"].get_bitmap().id();

    CHECK(menu.location().x() == 673);
    CHECK(menu.size().x() == 79);
    CHECK(menu.location().y() + menu.size().y() == 532);
    CHECK(system.location().x() == 721);
    CHECK(system.location().y() + 75 == 532);

    test_support::draws.clear();
    menu.draw(0.1f);
    const auto* top = find_draw(main_top_id);
    const auto* tile = find_draw(main_tile_id);
    const auto* bottom = find_draw(main_bottom_id);
    REQUIRE(top);
    REQUIRE(tile);
    REQUIRE(bottom);
    CHECK(top->bounds.width() == 79);
    CHECK(top->bounds.height() == 34);
    CHECK(tile->bounds.height() == 74);
    CHECK(bottom->bounds.height() == 41);
    CHECK(top->opacity == 1.0f);
    CHECK(tile->opacity == 1.0f);
    CHECK(bottom->opacity == 1.0f);
    const auto low_alpha_background = test_support::draws;

    test_support::draws.clear();
    menu.draw(0.9f);
    CHECK(test_support::draws.size() == low_alpha_background.size());
    for (size_t i = 0; i < std::min(test_support::draws.size(), low_alpha_background.size()); ++i)
    {
        CHECK(test_support::draws[i].bitmap.id() == low_alpha_background[i].bitmap.id());
        CHECK(test_support::draws[i].bounds.getlt() == low_alpha_background[i].bounds.getlt());
        CHECK(test_support::draws[i].bounds.getrb() == low_alpha_background[i].bounds.getrb());
        CHECK(test_support::draws[i].opacity == 1.0f);
    }

    test_support::draws.clear();
    system.draw(0.9f);
    top = find_draw(system_top_id);
    bottom = find_draw(system_bottom_id);
    REQUIRE(top);
    REQUIRE(bottom);
    CHECK(top->bounds.t() == system.location().y());
    CHECK(bottom->bounds.t() == system.location().y() + 34);
    CHECK(top->opacity == 1.0f);
    CHECK(bottom->opacity == 1.0f);
    CHECK_FALSE(find_draw(system_tile_id));
    test_support::snapshot(test_support::artifact("hud-system-menu.ppm"), 800, 600);

    test_support::draws.clear();
    menu.draw(0.1f);
    test_support::snapshot(test_support::artifact("hud-main-menu.ppm"), 800, 600);
}

TEST_CASE("Main menu dispatches one action per click and dismisses on Escape press")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Constants::set_viewsize(800, 600);
    actions.clear();

    ProbeMainMenu menu({712, 554});
    const auto equip_button = menu.button_center(0);
    REQUIRE(menu.is_active());
    ++press_id;
    CHECK(menu.send_cursor(true, equip_button).handled);
    CHECK((actions == std::vector<KeyAction::Id>{KeyAction::EQUIPS}));
    CHECK_FALSE(menu.is_active());

    menu.send_cursor(true, equip_button);
    menu.send_cursor(false, equip_button);
    CHECK((actions == std::vector<KeyAction::Id>{KeyAction::EQUIPS}));

    ProbeMainMenu untouched({712, 554});
    ++press_id;
    CHECK_FALSE(untouched.send_cursor(true, untouched.empty_panel_point()).handled);
    CHECK((actions == std::vector<KeyAction::Id>{KeyAction::EQUIPS}));
    CHECK(untouched.is_active());

    untouched.send_key(0, false, true);
    CHECK(untouched.is_active());
    untouched.send_key(0, true, false);
    CHECK(untouched.is_active());
    untouched.send_key(0, true, true);
    CHECK_FALSE(untouched.is_active());
}

TEST_CASE("System menu opens key settings and closes")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    Constants::set_viewsize(800, 600);
    actions.clear();

    ProbeSystemMenu menu({769, 554});
    const auto key_setting = menu.button_center(0);
    ++press_id;
    CHECK(menu.send_cursor(true, key_setting).handled);
    CHECK((actions == std::vector<KeyAction::Id>{KeyAction::KEYCONFIG}));
    CHECK_FALSE(menu.is_active());
}
