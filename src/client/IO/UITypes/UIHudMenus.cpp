#include "UIHudMenus.h"

#include "UINotice.h"
#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Constants.h"
#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        constexpr int16_t PANEL_WIDTH = 79;
        constexpr int16_t TOP_HEIGHT = 34;
        constexpr int16_t BOTTOM_HEIGHT = 41;
        constexpr int16_t BUTTON_HEIGHT = 25;
        constexpr int16_t MAIN_BUTTONS = 5;
        constexpr int16_t SYSTEM_BUTTONS = 2;
        constexpr int16_t main_height = MAIN_BUTTONS * BUTTON_HEIGHT + 24;
        constexpr int16_t system_height = std::max<int16_t>(
            TOP_HEIGHT + BOTTOM_HEIGHT, SYSTEM_BUTTONS * BUTTON_HEIGHT + 24);

        void draw_background(const Texture& top, const Texture& tile, const Texture& bottom,
            Point<int16_t> position, int16_t height)
        {
            top.draw(DrawArgument(position, 1.0f));
            const int16_t tile_height = std::max<int16_t>(0, height - TOP_HEIGHT - BOTTOM_HEIGHT);
            if (tile_height > 0)
                tile.draw(DrawArgument(position + Point<int16_t>(0, TOP_HEIGHT), Point<int16_t>(0, tile_height)));
            bottom.draw(DrawArgument(position + Point<int16_t>(0, height - BOTTOM_HEIGHT), 1.0f));
        }

        Point<int16_t> popup_position(Point<int16_t> anchor, int16_t height)
        {
            if (anchor.x() == 0 && anchor.y() == 0)
                anchor = { static_cast<int16_t>(Constants::viewwidth() / 2), Constants::viewheight() };

            return {
                static_cast<int16_t>(anchor.x() - PANEL_WIDTH / 2),
                static_cast<int16_t>(Constants::viewheight() - 68 - height)
            };
        }
    }

    UIMainMenu::UIMainMenu(Point<int16_t> anchor) : UIWindow({1, 0})
    {
        nl::node src = nl::nx::ui["StatusBar2.img"]["mainBar"]["Menu"];
        background_top = Texture(src["backgrnd"]["0"]);
        background_tile = Texture(src["backgrnd"]["1"]);
        background_bottom = Texture(src["backgrnd"]["2"]);

        buttons[EQUIPS] = std::make_unique<MapleButton>(src["BtEquip"], 8, 8);
        buttons[ITEMS] = std::make_unique<MapleButton>(src["BtItem"], 8, 8 + BUTTON_HEIGHT);
        buttons[QUESTS] = std::make_unique<MapleButton>(src["BtQuest"], 8, 8 + 2 * BUTTON_HEIGHT);
        buttons[SKILLS] = std::make_unique<MapleButton>(src["BtSkill"], 8, 8 + 3 * BUTTON_HEIGHT);
        buttons[STATS] = std::make_unique<MapleButton>(src["BtStat"], 8, 8 + 4 * BUTTON_HEIGHT);

        dimension = { PANEL_WIDTH, main_height };
        set_default_position(popup_position(anchor, main_height));
    }

    Button::State UIMainMenu::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case EQUIPS: UI::get().send_menu(KeyAction::EQUIPS); break;
        case ITEMS: UI::get().send_menu(KeyAction::INVENTORY); break;
        case QUESTS: UI::get().send_menu(KeyAction::QUESTLOG); break;
        case SKILLS: UI::get().send_menu(KeyAction::SKILLBOOK); break;
        case STATS: UI::get().send_menu(KeyAction::CHARSTATS); break;
        default: break;
        }

        active = false;
        return Button::NORMAL;
    }

    void UIMainMenu::draw(float) const
    {
        draw_background(background_top, background_tile, background_bottom, position, dimension.y());
        UIWindow::draw(1.0f);
    }

    void UIMainMenu::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            active = false;
    }

    UISystemMenu::UISystemMenu(Point<int16_t> anchor) : UIWindow({1, 0})
    {
        nl::node src = nl::nx::ui["StatusBar2.img"]["mainBar"]["System"];
        background_top = Texture(src["backgrnd"]["0"]);
        background_tile = Texture(src["backgrnd"]["1"]);
        background_bottom = Texture(src["backgrnd"]["2"]);

        buttons[KEYSETTING] = std::make_unique<MapleButton>(src["BtKeySetting"], 8, 8);
        buttons[QUIT] = std::make_unique<MapleButton>(src["BtGameQuit"], 8, 8 + BUTTON_HEIGHT);

        dimension = { PANEL_WIDTH, system_height };
        set_default_position(popup_position(anchor, system_height));
    }

    Button::State UISystemMenu::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case KEYSETTING:
            UI::get().send_menu(KeyAction::KEYCONFIG);
            break;
        case QUIT:
            UI::get().emplace<UIYesNo>("Quit game?", [](bool yes)
            {
                if (yes)
                    UI::get().quit();
            });
            break;
        default:
            break;
        }

        active = false;
        return Button::NORMAL;
    }

    void UISystemMenu::draw(float) const
    {
        draw_background(background_top, background_tile, background_bottom, position, dimension.y());
        UIWindow::draw(1.0f);
    }

    void UISystemMenu::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            active = false;
    }
}
