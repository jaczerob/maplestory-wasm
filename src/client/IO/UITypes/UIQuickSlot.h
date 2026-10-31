#pragma once

#include "../UIWindow.h"
#include "../Keyboard.h"

#include <map>

namespace jrc
{
    class UIQuickSlot : public UIWindow
    {
    public:
        static constexpr Type TYPE = QUICKSLOTCONFIG;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        explicit UIQuickSlot(Keyboard& keyboard);

        void draw(float alpha) const override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        bool assign_key(int32_t glfw_key);
        bool is_capturing() const;

    protected:
        Button::State button_pressed(uint16_t buttonid) override;
        CursorResult send_window_cursor(bool clicked, Point<int16_t> cursorpos) override;

    private:
        int slot_by_position(Point<int16_t> cursorpos) const;
        void reset();
        void apply();

        Keyboard& keyboard;
        Keyboard::Quickslots staged_keys;
        std::map<uint8_t, Texture> key_labels;
        int selected_slot = -1;
        bool dirty = false;

        enum Buttons : uint16_t { DEFAULT, CANCEL, OK };
    };
}
