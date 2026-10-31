//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UIStatusBar.h"
#include "UIHudMenus.h"
#include "UIKeyConfig.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Character/ExpTable.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Data/SkillData.h"
#include "../../Graphics/Geometry.h"

#include "nlnx/nx.hpp"


namespace jrc
{
    constexpr Point<int16_t> UIStatusbar::POSITION;
    constexpr Point<int16_t> UIStatusbar::DIMENSION;

    namespace
    {
        uint8_t quickslot_label_key(uint8_t key)
        {
            switch (key)
            {
            case 85: return 29;
            case 86: return 42;
            case 87: return 56;
            default: return key;
            }
        }
    }

    UIStatusbar::UIStatusbar(const CharStats& st)
        : UIElement(POSITION, DIMENSION), stats(st), keyboard(&UI::get().get_keyboard()), chatbar(POSITION)
    {
        nl::node mainbar = nl::nx::ui["StatusBar2.img"]["mainBar"];

        sprites.emplace_back(mainbar["backgrnd"]);
        sprites.emplace_back(mainbar["gaugeBackgrd"]);
        sprites.emplace_back(mainbar["notice"]);
        sprites.emplace_back(mainbar["lvBacktrnd"]);
        sprites.emplace_back(mainbar["lvCover"]);

        expbar = {
            mainbar.resolve("gauge/exp/0"),
            mainbar.resolve("gauge/exp/1"),
            mainbar.resolve("gauge/exp/2"),
            308, 0.0f
        };
        hpbar = {
            mainbar.resolve("gauge/hp/0"),
            mainbar.resolve("gauge/hp/1"),
            mainbar.resolve("gauge/hp/2"),
            137, 0.0f
        };
        mpbar = {
            mainbar.resolve("gauge/mp/0"),
            mainbar.resolve("gauge/mp/1"),
            mainbar.resolve("gauge/mp/2"),
            137, 0.0f
        };

        statset   = { mainbar["gauge"]["number"], Charset::RIGHT };
        levelset  = { mainbar["lvNumber"],        Charset::LEFT  };

        joblabel  = { Text::A11M, Text::LEFT, Text::YELLOW };
        namelabel = { Text::A13M, Text::LEFT, Text::WHITE  };

        buttons[BT_WHISPER]   = std::make_unique<MapleButton>(mainbar["BtChat"]);
        buttons[BT_CALLGM]    = std::make_unique<MapleButton>(mainbar["BtClaim"]);

        buttons[BT_CASHSHOP]  = std::make_unique<MapleButton>(mainbar["BtCashShop"]);
        buttons[BT_TRADE]     = std::make_unique<MapleButton>(mainbar["BtMTS"]);
        buttons[BT_MENU]      = std::make_unique<MapleButton>(mainbar["BtMenu"]);
        buttons[BT_OPTIONS]   = std::make_unique<MapleButton>(mainbar["BtSystem"]);

        buttons[BT_CHARACTER] = std::make_unique<MapleButton>(mainbar["BtCharacter"]);
        buttons[BT_STATS]     = std::make_unique<MapleButton>(mainbar["BtStat"]);
        buttons[BT_QUEST]     = std::make_unique<MapleButton>(mainbar["BtQuest"]);
        buttons[BT_INVENTORY] = std::make_unique<MapleButton>(mainbar["BtInven"]);
        buttons[BT_EQUIPS]    = std::make_unique<MapleButton>(mainbar["BtEquip"]);
        buttons[BT_SKILL]     = std::make_unique<MapleButton>(mainbar["BtSkill"]);

        nl::node quickslot_src = mainbar["quickSlot"];
        quickslot = Texture(quickslot_src["quickSlot"]);
        buttons[BT_QSOPEN] = std::make_unique<MapleButton>(quickslot_src["BtOpen"]);
        buttons[BT_QSCLOSE] = std::make_unique<MapleButton>(quickslot_src["BtClose"]);
        buttons[BT_QSCLOSE]->set_active(false);

        nl::node key_config = nl::nx::ui["UIWindow.img"]["KeyConfig"];
        nl::node quickslot_labels = key_config["quickslotConfig"]["key"];
        for (int key = 1; key < 256; ++key)
        {
            if (!Keyboard::is_bindable_key(static_cast<uint8_t>(key)))
            {
                continue;
            }

            const std::string name = std::to_string(key);
            nl::node label = quickslot_labels[name];
            if (!label)
            {
                label = key_config["key"][name];
            }
            if (label)
            {
                quickslot_key_labels.emplace(static_cast<uint8_t>(key), Texture(label));
            }
        }

        update_layout_position();
        update_quickslot_icons();
    }

    void UIStatusbar::draw(float alpha) const
    {
        if (quickslot_open)
        {
            quickslot.draw(DrawArgument(position));
            draw_quickslot_icons();
        }

        draw_sprites(alpha);
        draw_buttons(alpha);

        expbar.draw(position + Point<int16_t>(-261, -15));
        hpbar.draw(position + Point<int16_t>(-261, -31));
        mpbar.draw(position + Point<int16_t>(-90, -31));

        int16_t level = stats.get_stat(Maplestat::LEVEL);
        int16_t hp    = stats.get_stat(Maplestat::HP);
        int16_t mp    = stats.get_stat(Maplestat::MP);
        int32_t maxhp = stats.get_total(Equipstat::HP);
        int32_t maxmp = stats.get_total(Equipstat::MP);
        int64_t exp   = stats.get_exp();

        std::string expstring = std::to_string(100 * getexppercent());
        statset.draw(
            std::to_string(exp) + "[" + expstring.substr(0, expstring.find('.') + 3) + "%]",
            position + Point<int16_t>(47, -13)
        );
        statset.draw(
            "[" + std::to_string(hp) + "/" + std::to_string(maxhp) + "]",
            position + Point<int16_t>(-124, -29)
        );
        statset.draw(
            "[" + std::to_string(mp) + "/" + std::to_string(maxmp) + "]",
            position + Point<int16_t>(47, -29)
        );
        levelset.draw(
            std::to_string(level),
            position + Point<int16_t>(-480, -24)
        );

        joblabel.draw(position + Point<int16_t>(-435, -21));
        namelabel.draw(position + Point<int16_t>(-435, -36));

        chatbar.draw(alpha);
    }

    void UIStatusbar::update()
    {
        update_layout_position();
        UIElement::update();

        chatbar.update();

        expbar.update(getexppercent());
        hpbar.update(gethppercent());
        mpbar.update(getmppercent());

        namelabel.change_text(stats.get_name());
        joblabel.change_text(stats.get_jobname());

        for (auto iter : message_cooldowns)
        {
            iter.second -= Constants::TIMESTEP;
        }

        update_quickslot_icons();
    }

    void UIStatusbar::update_layout_position()
    {
        position = {
            POSITION.x(),
            static_cast<int16_t>(Constants::viewheight() - Constants::VIEWYOFFSET)
        };
        chatbar.set_position(position);
    }

    Button::State UIStatusbar::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_TRADE:
            UI::get().send_menu(KeyAction::MTS);
            return Button::NORMAL;
        case BT_MENU:
            UI::get().send_menu(KeyAction::MAINMENU);
            return Button::NORMAL;
        case BT_OPTIONS:
            UI::get().emplace<UISystemMenu>(system_menu_anchor());
            return Button::NORMAL;
        case BT_QSOPEN:
            toggle_quickslot();
            return Button::NORMAL;
        case BT_QSCLOSE:
            toggle_quickslot();
            return Button::NORMAL;
        case BT_CASHSHOP:
            UI::get().send_menu(KeyAction::CASHSHOP);
            return Button::NORMAL;
        case BT_STATS:
            UI::get().send_menu(KeyAction::CHARSTATS);
            return Button::NORMAL;
        case BT_QUEST:
            UI::get().send_menu(KeyAction::QUESTLOG);
            return Button::NORMAL;
        case BT_INVENTORY:
            UI::get().send_menu(KeyAction::INVENTORY);
            return Button::NORMAL;
        case BT_EQUIPS:
            UI::get().send_menu(KeyAction::EQUIPS);
            return Button::NORMAL;
        case BT_SKILL:
            UI::get().send_menu(KeyAction::SKILLBOOK);
            return Button::NORMAL;
        default:
            return Button::PRESSED;
        }
    }

    void UIStatusbar::toggle_quickslot()
    {
        quickslot_open = !quickslot_open;
        buttons[BT_QSOPEN]->set_active(!quickslot_open);
        buttons[BT_QSCLOSE]->set_active(quickslot_open);
        buttons[BT_QSOPEN]->set_state(Button::NORMAL);
        buttons[BT_QSCLOSE]->set_state(Button::NORMAL);
    }

    Point<int16_t> UIStatusbar::menu_anchor() const
    {
        const Rectangle<int16_t> bounds = buttons.at(BT_MENU)->bounds(position);
        return { static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                 static_cast<int16_t>((bounds.t() + bounds.b()) / 2) };
    }

    Point<int16_t> UIStatusbar::system_menu_anchor() const
    {
        const Rectangle<int16_t> bounds = buttons.at(BT_OPTIONS)->bounds(position);
        return { static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                 static_cast<int16_t>((bounds.t() + bounds.b()) / 2) };
    }

    bool UIStatusbar::is_in_range(Point<int16_t> cursorpos) const
    {
        Rectangle<int16_t> bounds(
            position - Point<int16_t>(512, 84),
            position - Point<int16_t>(512, 84) + dimension
        );

        if (bounds.contains(cursorpos) || chatbar.is_in_range(cursorpos))
            return true;

        if (quickslot_open && quickslot_bounds().contains(cursorpos))
            return true;

        for (const auto& button : buttons)
            if (button.second->is_active() && button.second->bounds(position).contains(cursorpos))
                return true;

        return false;
    }

    bool UIStatusbar::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (chatbar.remove_cursor(clicked, cursorpos))
        {
            return true;
        }

        return UIElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIStatusbar::send_cursor(bool pressed, Point<int16_t> cursorpos)
    {
        if (chatbar.is_in_range(cursorpos))
        {
            if (CursorResult child_result = chatbar.send_cursor(pressed, cursorpos))
            {
                return child_result;
            }

            return UIElement::send_cursor(pressed, cursorpos);
        }

        chatbar.send_cursor(pressed, cursorpos);
        if (CursorResult result = UIElement::send_cursor(pressed, cursorpos))
        {
            return result;
        }

        if (quickslot_open && quickslot_bounds().contains(cursorpos))
        {
            return {Cursor::IDLE, true};
        }
        return {};
    }

    void UIStatusbar::send_icon(const Icon& dropped, Point<int16_t> cursorpos)
    {
        if (!quickslot_open)
        {
            return;
        }

        int slot = quickslot_by_position(cursorpos);
        Keyboard::Mapping mapping = dropped.get_key_mapping();
        if (slot < 0 || (mapping.type != KeyType::SKILL && mapping.type != KeyType::ITEM))
        {
            return;
        }

        UIKeyConfig::assign_quickslot_mapping(keyboard->get_quickslots()[slot], mapping);
    }

    void UIStatusbar::update_quickslot_icons()
    {
        const Keyboard::Quickslots& keys = keyboard->get_quickslots();
        nl::node action_icons = nl::nx::ui["UIWindow.img"]["KeyConfig"]["icon"];

        for (size_t slot = 0; slot < keys.size(); ++slot)
        {
            Keyboard::Mapping mapping = keyboard->get_maple_mapping(keys[slot]);
            if (quickslot_icons_loaded && displayed_quickslots[slot] == keys[slot] &&
                quickslot_mappings[slot] == mapping)
            {
                continue;
            }

            displayed_quickslots[slot] = keys[slot];
            quickslot_mappings[slot] = mapping;
            quickslot_icons[slot].reset();

            Texture texture;
            if (mapping.type == KeyType::SKILL && mapping.action > 0)
            {
                texture = SkillData::get(mapping.action).get_icon(SkillData::NORMAL);
            }
            else if (mapping.type == KeyType::ITEM && mapping.action > 0)
            {
                const ItemData& item = ItemData::get(mapping.action);
                if (item)
                {
                    texture = item.get_icon(false);
                }
            }
            else if ((mapping.type == KeyType::MENU || mapping.type == KeyType::ACTION || mapping.type == KeyType::FACE) &&
                     mapping.action >= 0 && mapping.action <= 106)
            {
                nl::node icon = action_icons[std::to_string(mapping.action)];
                if (icon)
                {
                    texture = Texture(icon);
                }
            }

            if (texture.is_valid())
            {
                quickslot_icons[slot] = std::make_unique<Icon>(
                    std::make_unique<Icon::NullType>(), texture, -1
                );
            }
        }

        quickslot_icons_loaded = true;
    }

    void UIStatusbar::draw_quickslot_icons() const
    {
        const Point<int16_t> origin = position - quickslot.get_origin();
        static const Text scroll_lock_label(Text::A11M, Text::CENTER, Text::WHITE, "ScrLk");
        for (size_t slot = 0; slot < quickslot_icons.size(); ++slot)
        {
            Point<int16_t> slot_position = origin + Point<int16_t>(
                7 + static_cast<int16_t>(slot % 4) * 33,
                16 + static_cast<int16_t>(slot / 4) * 33
            );

            if (quickslot_icons[slot])
            {
                quickslot_icons[slot]->draw(slot_position);
            }

            if (displayed_quickslots[slot] == 88)
            {
                ColorBox(28, 11, Geometry::BLACK, 0.6f).draw(slot_position);
                scroll_lock_label.draw(slot_position + Point<int16_t>(14, 6));
            }
            else if (auto label = quickslot_key_labels.find(quickslot_label_key(displayed_quickslots[slot]));
                     label != quickslot_key_labels.end())
            {
                ColorBox(label->second.width(), label->second.height(), Geometry::BLACK, 0.6f).draw(slot_position);
                label->second.draw(DrawArgument(slot_position));
            }
        }
    }

    int UIStatusbar::quickslot_by_position(Point<int16_t> cursorpos) const
    {
        const Point<int16_t> origin = position - quickslot.get_origin();
        for (int slot = 0; slot < 8; ++slot)
        {
            Point<int16_t> slot_position = origin + Point<int16_t>(
                7 + static_cast<int16_t>(slot % 4) * 33,
                16 + static_cast<int16_t>(slot / 4) * 33
            );
            if (Rectangle<int16_t>(slot_position, slot_position + Point<int16_t>(30, 30)).contains(cursorpos))
            {
                return slot;
            }
        }
        return -1;
    }

    Rectangle<int16_t> UIStatusbar::quickslot_bounds() const
    {
        const Point<int16_t> top_left = position - quickslot.get_origin();
        return {top_left, top_left + quickslot.get_dimensions()};
    }

    void UIStatusbar::send_chatline(const std::string& line, UIChatbar::LineType type)
    {
        chatbar.send_line(line, type);
    }

    void UIStatusbar::focus_chatfield()
    {
        chatbar.focus_chatfield();
    }

    void UIStatusbar::set_chat_target(UIChatbar::ChatTarget target)
    {
        chatbar.set_chat_target(target);
    }

    void UIStatusbar::cycle_chat_target()
    {
        chatbar.cycle_chat_target();
    }

    void UIStatusbar::set_pending_party_invite(int32_t party_id, const std::string& inviter)
    {
        chatbar.set_pending_party_invite(party_id, inviter);
    }

    void UIStatusbar::clear_pending_party_invite()
    {
        chatbar.clear_pending_party_invite();
    }

    void UIStatusbar::set_party_state(int32_t party_id, int32_t leader_id, const std::vector<UIChatbar::PartyMember>& members)
    {
        chatbar.set_party_state(party_id, leader_id, members);
    }

    void UIStatusbar::clear_party_state()
    {
        chatbar.clear_party_state();
    }

    void UIStatusbar::set_party_leader(int32_t leader_id)
    {
        chatbar.set_party_leader(leader_id);
    }

    void UIStatusbar::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
    {
        chatbar.update_party_member_hp(cid, hp, max_hp);
    }

    int32_t UIStatusbar::get_party_id() const
    {
        return chatbar.get_party_id();
    }

    int32_t UIStatusbar::get_party_leader_id() const
    {
        return chatbar.get_party_leader_id();
    }

    int32_t UIStatusbar::get_pending_party_invite_id() const
    {
        return chatbar.get_pending_party_invite_id();
    }

    const std::string& UIStatusbar::get_pending_party_inviter() const
    {
        return chatbar.get_pending_party_inviter();
    }

    const std::vector<UIChatbar::PartyMember>& UIStatusbar::get_party_members() const
    {
        return chatbar.get_party_members();
    }

    void UIStatusbar::display_message(Messages::Type line, UIChatbar::LineType type)
    {
        if (message_cooldowns[line] > 0)
        {
            return;
        }

        std::string message{ Messages::messages[line] };
        chatbar.send_line(message, type);

        message_cooldowns[line] = MESSAGE_COOLDOWN;
    }

    float UIStatusbar::getexppercent() const
    {
        int16_t level = stats.get_stat(Maplestat::LEVEL);
        if (level >= ExpTable::LEVELCAP)
        {
            return 0.0f;
        }

        int64_t exp = stats.get_exp();
        return static_cast<float>(
            static_cast<double>(exp) / ExpTable::values[level]
        );
    }

    float UIStatusbar::gethppercent() const
    {
        int16_t hp = stats.get_stat(Maplestat::HP);
        int32_t maxhp = stats.get_total(Equipstat::HP);

        return static_cast<float>(hp) / maxhp;
    }

    float UIStatusbar::getmppercent() const
    {
        int16_t mp = stats.get_stat(Maplestat::MP);
        int32_t maxmp = stats.get_total(Equipstat::MP);

        return static_cast<float>(mp) / maxmp;
    }
}
