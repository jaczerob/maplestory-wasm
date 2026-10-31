#include "support/assets.h"
#include "support/render_capture.h"
#include <doctest/doctest.h>

#include "client/Character/CharStats.h"
#include "client/Graphics/GraphicsGL.h"
#include "client/IO/UI.h"
#include "client/IO/UITypes/UIHudMenus.h"
#include "client/IO/UITypes/UIKeyConfig.h"
#include "client/IO/UITypes/UIStatusBar.h"
#include "client/Audio/Audio.h"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <vector>

namespace
{
    using namespace jrc;

    struct AssignedQuickslot
    {
        uint8_t key;
        Keyboard::Mapping mapping;
    };

    std::vector<AssignedQuickslot> assigned;

    class ProbeIconType : public Icon::Type
    {
    public:
        explicit ProbeIconType(Keyboard::Mapping mapping) : mapping(mapping) {}
        void drop_on_stage() const override {}
        void drop_on_equips(Equipslot::Id) const override {}
        void drop_on_items(InventoryType::Id, Equipslot::Id, int16_t, bool) const override {}
        Keyboard::Mapping get_key_mapping() const override { return mapping; }

    private:
        Keyboard::Mapping mapping;
    };

    class ProbeStatusbar : public UIStatusbar
    {
    public:
        using UIStatusbar::UIStatusbar;

        Point<int16_t> slot_point(int slot, uint32_t quickslot_bitmap) const
        {
            const auto found = std::find_if(test_support::draws.begin(), test_support::draws.end(),
                [quickslot_bitmap](const auto& draw) { return draw.bitmap.id() == quickslot_bitmap; });
            REQUIRE(found != test_support::draws.end());
            const auto& rect = found->bounds;
            return {
                static_cast<int16_t>(rect.l() + 7 + slot % 4 * 33 + 15),
                static_cast<int16_t>(rect.t() + 16 + slot / 4 * 33 + 15)
            };
        }

        void render() const
        {
            test_support::draws.clear();
            draw(1.0f);
        }
    };

    size_t draw_index(uint32_t bitmap_id)
    {
        const auto found = std::find_if(test_support::draws.begin(), test_support::draws.end(),
            [bitmap_id](const auto& draw) { return draw.bitmap.id() == bitmap_id; });
        return found == test_support::draws.end()
            ? test_support::draws.size()
            : static_cast<size_t>(found - test_support::draws.begin());
    }

    CharStats make_stats()
    {
        StatsEntry entry{};
        entry.name = "quickslot";
        entry.stats[Maplestat::LEVEL] = 10;
        entry.stats[Maplestat::MAXHP] = 100;
        entry.stats[Maplestat::MAXMP] = 100;
        entry.stats[Maplestat::HP] = 100;
        entry.stats[Maplestat::MP] = 100;
        return CharStats(entry);
    }
}

namespace jrc
{
    GraphicsGL::GraphicsGL() : locked(false) {}
    Text::Layout GraphicsGL::createlayout(const std::string&, Text::Font, Text::Alignment, int16_t, bool) { return {}; }
    void GraphicsGL::drawtext(const DrawArgument&, const std::string&, const Text::Layout&, Text::Font,
        Text::Color, Text::Background, const Range<int16_t>*) {}
    void GraphicsGL::drawrectangle(int16_t, int16_t, int16_t, int16_t, float, float, float, float) {}

    UI::UI() : cursor_press_id(0), enabled(true), quitted(false) {}
    Keyboard& UI::get_keyboard() { return keyboard; }
    uint64_t UI::get_cursor_press_id() const { return cursor_press_id; }
    void UI::send_menu(KeyAction::Id) {}

    UISystemMenu::UISystemMenu(Point<int16_t>) : UIWindow({1, 0}) {}
    void UISystemMenu::send_key(int32_t, bool, bool) {}
    void UISystemMenu::draw(float) const {}
    Button::State UISystemMenu::button_pressed(uint16_t) { return Button::NORMAL; }

    Cursor::Cursor() : state(IDLE), hide_counter(0) {}
    ScrollingNotice::ScrollingNotice() : active(false) {}
    Sound::Sound(Name) : id(0) {}
    void Sound::play() const {}

    Textfield::Textfield() = default;
    Textfield::~Textfield() = default;
    Slider::Slider() = default;
    UIChatbar::UIChatbar(Point<int16_t> position) : UIElement(position, {}) {}
    void UIChatbar::draw(float) const {}
    void UIChatbar::update() {}
    void UIChatbar::set_position(Point<int16_t> pos) { position = pos; }
    bool UIChatbar::is_in_range(Point<int16_t>) const { return false; }
    bool UIChatbar::remove_cursor(bool, Point<int16_t>) { return false; }
    UIElement::CursorResult UIChatbar::send_cursor(bool, Point<int16_t>) { return {}; }
    void UIChatbar::send_line(const std::string&, LineType) {}
    void UIChatbar::focus_chatfield() {}
    void UIChatbar::set_chat_target(ChatTarget) {}
    void UIChatbar::cycle_chat_target() {}
    void UIChatbar::set_pending_party_invite(int32_t, const std::string&) {}
    void UIChatbar::clear_pending_party_invite() {}
    void UIChatbar::set_party_state(int32_t, int32_t, const std::vector<PartyMember>&) {}
    void UIChatbar::clear_party_state() {}
    void UIChatbar::set_party_leader(int32_t) {}
    void UIChatbar::update_party_member_hp(int32_t, int32_t, int32_t) {}
    int32_t UIChatbar::get_party_id() const { return 0; }
    int32_t UIChatbar::get_party_leader_id() const { return 0; }
    int32_t UIChatbar::get_pending_party_invite_id() const { return 0; }
    const std::string& UIChatbar::get_pending_party_inviter() const { static const std::string empty; return empty; }
    const std::vector<UIChatbar::PartyMember>& UIChatbar::get_party_members() const
    {
        static const std::vector<UIChatbar::PartyMember> empty;
        return empty;
    }
    Button::State UIChatbar::button_pressed(uint16_t) { return Button::NORMAL; }

    void UIKeyConfig::assign_quickslot_mapping(uint8_t key, Keyboard::Mapping mapping)
    {
        assigned.push_back({key, mapping});
    }
}

TEST_CASE("Status bar quick slots draw beneath HUD controls and reflect current skill bindings")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    test_support::NxFile skill("Skill.nx", nl::nx::skill);
    test_support::NxFile strings("String.nx", nl::nx::string);
    test_support::NxFile item("Item.nx", nl::nx::item);
    Constants::set_viewsize(800, 600);

    Keyboard& keyboard = UI::get().get_keyboard();
    keyboard.set_quickslots(Keyboard::DEFAULT_QUICKSLOTS);
    keyboard.assign(42, KeyType::SKILL, 1001005);
    CharStats stats = make_stats();
    ProbeStatusbar bar(stats);
    bar.toggle_quickslot();
    bar.render();

    const auto mainbar = nl::nx::ui["StatusBar2.img"]["mainBar"];
    const auto quickslot_src = mainbar["quickSlot"];
    const uint32_t background_id = quickslot_src["quickSlot"].get_bitmap().id();
    const uint32_t icon_id = nl::nx::skill["100.img"]["skill"]["1001005"]["icon"]
        .get_bitmap().id();
    nl::node key_config = nl::nx::ui["UIWindow.img"]["KeyConfig"];
    nl::node key_label = key_config["quickslotConfig"]["key"]["42"];
    if (!key_label)
        key_label = key_config["key"]["42"];
    const uint32_t key_label_id = key_label.get_bitmap().id();
    const uint32_t bar_id = mainbar["backgrnd"].get_bitmap().id();
    const uint32_t close_id = quickslot_src["BtClose"]["normal"]["0"].get_bitmap().id();

    REQUIRE(background_id > 0);
    REQUIRE(icon_id > 0);
    REQUIRE(key_label_id > 0);
    REQUIRE(bar_id > 0);
    REQUIRE(close_id > 0);
    CHECK(draw_index(background_id) < draw_index(icon_id));
    CHECK(draw_index(icon_id) < draw_index(key_label_id));
    CHECK(draw_index(key_label_id) < draw_index(bar_id));
    CHECK(draw_index(bar_id) < draw_index(close_id));
    test_support::snapshot(test_support::artifact("quickslot-hud.ppm"), 800, 600);

    keyboard.assign(42, KeyType::SKILL, 1001004);
    bar.update();
    bar.render();
    const uint32_t changed_icon_id = nl::nx::skill["100.img"]["skill"]["1001004"]["icon"]
        .get_bitmap().id();
    REQUIRE(changed_icon_id > 0);
    CHECK(changed_icon_id != icon_id);
    CHECK(draw_index(changed_icon_id) < draw_index(bar_id));
}

TEST_CASE("Status bar only forwards skill and item drops onto open quick slot cells")
{
    using namespace jrc;
    test_support::NxFile ui("UI.nx", nl::nx::ui);
    test_support::NxFile skill("Skill.nx", nl::nx::skill);
    test_support::NxFile strings("String.nx", nl::nx::string);
    test_support::NxFile item("Item.nx", nl::nx::item);
    Constants::set_viewsize(800, 600);

    Keyboard& keyboard = UI::get().get_keyboard();
    keyboard.set_quickslots(Keyboard::DEFAULT_QUICKSLOTS);
    CharStats stats = make_stats();
    ProbeStatusbar bar(stats);
    bar.toggle_quickslot();
    bar.render();
    const uint32_t background_id = nl::nx::ui["StatusBar2.img"]["mainBar"]["quickSlot"]["quickSlot"]
        .get_bitmap().id();
    const Point<int16_t> slot = bar.slot_point(0, background_id);
    const Point<int16_t> outside = {static_cast<int16_t>(slot.x() - 24), slot.y()};

    Icon skill_icon(std::make_unique<ProbeIconType>(Keyboard::Mapping{KeyType::SKILL, 1001005}), {}, -1);
    Icon item_icon(std::make_unique<ProbeIconType>(Keyboard::Mapping{KeyType::ITEM, 2000000}), {}, -1);
    Icon unsupported(std::make_unique<ProbeIconType>(Keyboard::Mapping{KeyType::MENU, KeyAction::INVENTORY}), {}, -1);
    assigned.clear();

    bar.send_icon(skill_icon, slot);
    REQUIRE(assigned.size() == 1);
    CHECK(assigned.back().key == Keyboard::DEFAULT_QUICKSLOTS[0]);
    CHECK(assigned.back().mapping == (Keyboard::Mapping{KeyType::SKILL, 1001005}));
    bar.send_icon(item_icon, slot);
    REQUIRE(assigned.size() == 2);
    CHECK(assigned.back().mapping == (Keyboard::Mapping{KeyType::ITEM, 2000000}));

    bar.send_icon(skill_icon, outside);
    bar.send_icon(unsupported, slot);
    CHECK(assigned.size() == 2);

    bar.toggle_quickslot();
    bar.send_icon(skill_icon, slot);
    CHECK(assigned.size() == 2);
}
