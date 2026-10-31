#pragma once
#include "../UIWindow.h"

#include "../../Graphics/Texture.h"

namespace jrc
{
    class UIMainMenu : public UIWindow
    {
    public:
        static constexpr Type TYPE = MAINMENU;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        explicit UIMainMenu(Point<int16_t> anchor = {});
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        void draw(float alpha) const override;

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum Buttons : uint16_t { EQUIPS, ITEMS, QUESTS, SKILLS, STATS };
        Texture background_top;
        Texture background_tile;
        Texture background_bottom;
    };

    class UISystemMenu : public UIWindow
    {
    public:
        static constexpr Type TYPE = SYSTEMMENU;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        explicit UISystemMenu(Point<int16_t> anchor = {});
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        void draw(float alpha) const override;

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum Buttons : uint16_t { KEYSETTING, QUIT };
        Texture background_top;
        Texture background_tile;
        Texture background_bottom;
    };
}
