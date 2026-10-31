#include "UIQuickSlot.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Constants.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include "../../Net/Packets/PlayerPackets.h"

#include "nlnx/nx.hpp"

#include <string>

namespace jrc
{
    namespace
    {
        constexpr int16_t SLOT_X = 64;
        constexpr int16_t SLOT_Y = 113;
        constexpr int16_t SLOT_X_STEP = 35;
        constexpr int16_t SLOT_Y_STEP = 34;
        constexpr int16_t SLOT_WIDTH = 30;
        constexpr int16_t SLOT_HEIGHT = 28;

        uint8_t label_key(uint8_t key)
        {
            switch (key)
            {
            case 85: return 29;
            case 86: return 42;
            case 87: return 56;
            default: return key;
            }
        }

        std::unique_ptr<MapleButton> button_at(nl::node source, Point<int16_t> top_left)
        {
            Texture normal(source["normal"]["0"]);
            return std::make_unique<MapleButton>(source, top_left + normal.get_origin());
        }
    }

    UIQuickSlot::UIQuickSlot(Keyboard& in_keyboard)
        : UIWindow({0, 20}), keyboard(in_keyboard), staged_keys(keyboard.get_quickslots())
    {
        nl::node src = nl::nx::ui["UIWindow.img"]["KeyConfig"];
        nl::node config = src["quickslotConfig"];
        Texture background(config["backgrnd"]);
        sprites.emplace_back(config["backgrnd"], DrawArgument(background.get_origin()));
        dimension = background.get_dimensions();

        nl::node button_src = nl::nx::ui["UIWindow2.img"]["KeyConfig"];
        buttons[DEFAULT] = button_at(button_src["BtDefault"], {10, 211});
        buttons[OK] = button_at(button_src["BtOK"], {156, 211});
        buttons[CANCEL] = button_at(button_src["BtCancel"], {206, 211});

        for (int key = 1; key < 256; ++key)
        {
            if (!Keyboard::is_bindable_key(static_cast<uint8_t>(key)))
            {
                continue;
            }

            const std::string name = std::to_string(key);
            nl::node label = config["key"][name];
            if (!label)
            {
                label = src["key"][name];
            }
            if (label)
            {
                key_labels.emplace(static_cast<uint8_t>(key), Texture(label));
            }
        }

        set_default_position({
            static_cast<int16_t>((Constants::viewwidth() - dimension.x()) / 2),
            static_cast<int16_t>((Constants::viewheight() - dimension.y()) / 2)
        });
    }

    void UIQuickSlot::draw(float) const
    {
        UIElement::draw(1.0f);
        static const Text scroll_lock_label(Text::A11M, Text::CENTER, Text::BLACK, "ScrLk");

        for (size_t slot = 0; slot < staged_keys.size(); ++slot)
        {
            Point<int16_t> slot_position = position + Point<int16_t>(
                SLOT_X + static_cast<int16_t>(slot % 4) * SLOT_X_STEP,
                SLOT_Y + static_cast<int16_t>(slot / 4) * SLOT_Y_STEP
            );

            if (static_cast<int>(slot) == selected_slot)
            {
                ColorBox(SLOT_WIDTH, SLOT_HEIGHT, Geometry::BLACK, 0.25f).draw(slot_position);
            }

            if (staged_keys[slot] == 88)
            {
                scroll_lock_label.draw(slot_position + Point<int16_t>(15, 14));
            }
            else if (auto label = key_labels.find(label_key(staged_keys[slot])); label != key_labels.end())
            {
                const Point<int16_t> label_size = label->second.get_dimensions();
                label->second.draw(DrawArgument(slot_position + Point<int16_t>(
                    static_cast<int16_t>((SLOT_WIDTH - label_size.x()) / 2),
                    static_cast<int16_t>((SLOT_HEIGHT - label_size.y()) / 2)
                )));
            }
        }
    }

    void UIQuickSlot::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            reset();
            active = false;
        }
    }

    bool UIQuickSlot::assign_key(int32_t glfw_key)
    {
        if (selected_slot < 0)
        {
            return false;
        }

        std::optional<uint8_t> key = keyboard.get_maple_key(glfw_key);
        if (!key)
        {
            return false;
        }

        *key = label_key(*key);

        const uint8_t previous = staged_keys[selected_slot];
        for (size_t slot = 0; slot < staged_keys.size(); ++slot)
        {
            if (slot != static_cast<size_t>(selected_slot) && staged_keys[slot] == *key)
            {
                staged_keys[slot] = previous;
            }
        }

        staged_keys[selected_slot] = *key;
        selected_slot = -1;
        dirty = staged_keys != keyboard.get_quickslots();
        return true;
    }

    bool UIQuickSlot::is_capturing() const
    {
        return selected_slot >= 0;
    }

    Button::State UIQuickSlot::button_pressed(uint16_t buttonid)
    {
        switch (buttonid)
        {
        case DEFAULT:
            staged_keys = Keyboard::DEFAULT_QUICKSLOTS;
            selected_slot = -1;
            dirty = staged_keys != keyboard.get_quickslots();
            break;
        case CANCEL:
            reset();
            active = false;
            break;
        case OK:
            apply();
            active = false;
            break;
        default:
            break;
        }
        return Button::NORMAL;
    }

    UIElement::CursorResult UIQuickSlot::send_window_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (CursorResult result = UIElement::send_cursor(clicked, cursorpos))
        {
            return result;
        }

        int slot = slot_by_position(cursorpos);
        if (slot >= 0)
        {
            if (clicked)
            {
                selected_slot = slot;
                return {Cursor::CLICKING, true};
            }
            return {Cursor::CANCLICK, true};
        }

        return {};
    }

    int UIQuickSlot::slot_by_position(Point<int16_t> cursorpos) const
    {
        Point<int16_t> relative = cursorpos - position;
        for (int slot = 0; slot < static_cast<int>(staged_keys.size()); ++slot)
        {
            Point<int16_t> top_left(
                SLOT_X + static_cast<int16_t>(slot % 4) * SLOT_X_STEP,
                SLOT_Y + static_cast<int16_t>(slot / 4) * SLOT_Y_STEP
            );
            if (Rectangle<int16_t>(top_left, top_left + Point<int16_t>(SLOT_WIDTH, SLOT_HEIGHT)).contains(relative))
            {
                return slot;
            }
        }
        return -1;
    }

    void UIQuickSlot::reset()
    {
        staged_keys = keyboard.get_quickslots();
        selected_slot = -1;
        dirty = false;
    }

    void UIQuickSlot::apply()
    {
        if (dirty)
        {
            ChangeQuickslotPacket(staged_keys).dispatch();
            keyboard.set_quickslots(staged_keys);
        }
        reset();
    }

}
