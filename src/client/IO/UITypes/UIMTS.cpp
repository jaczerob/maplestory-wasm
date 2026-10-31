#include "UIMTS.h"

#include "../Components/AreaButton.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/CashShop.h"
#include "../../Graphics/Geometry.h"
#include "../../Net/Packets/MTSPackets.h"

#include <algorithm>

namespace jrc
{
    namespace
    {
        enum ButtonId : uint16_t { EXIT = 1, PREVIOUS, NEXT, VIEW_FIRST = 10, CATEGORY_FIRST = 20 };
        const char* category_names[] = {"All", "Equipment", "Use", "Setup", "Etc", "Cash"};
        const char* view_names[] = {"Browse", "Transfers", "Sales"};
    }

    UIMTS::UIMTS()
        : background(CashShop::get().mts_skin()["Base"]["backgrnd"]),
          page_label(Text::A11M, Text::CENTER, Text::WHITE),
          balance_label(Text::A11M, Text::RIGHT, Text::WHITE),
          status(Text::A11M, Text::LEFT, Text::WHITE)
    {
        set_type(TYPE);
        dimension = {800, 600};
        update_screen(Constants::viewwidth(), Constants::viewheight());
        buttons[EXIT] = std::make_unique<AreaButton>(Point<int16_t>(690, 548), Point<int16_t>(80, 28));
        buttons[PREVIOUS] = std::make_unique<AreaButton>(Point<int16_t>(318, 548), Point<int16_t>(50, 28));
        buttons[NEXT] = std::make_unique<AreaButton>(Point<int16_t>(430, 548), Point<int16_t>(50, 28));
        for (uint16_t i = 0; i < views.size(); ++i)
        {
            const int16_t x = static_cast<int16_t>(160 + i * 160);
            buttons[VIEW_FIRST + i] = std::make_unique<AreaButton>(Point<int16_t>(x, 43), Point<int16_t>(150, 28));
            views[i] = {Text::A11M, Text::CENTER, i == 0 ? Text::YELLOW : Text::WHITE, view_names[i]};
        }
        for (uint16_t i = 0; i < categories.size(); ++i)
        {
            const int16_t x = static_cast<int16_t>(42 + i * 120);
            buttons[CATEGORY_FIRST + i] = std::make_unique<AreaButton>(Point<int16_t>(x, 78), Point<int16_t>(112, 30));
            categories[i] = {Text::A11M, Text::CENTER, i == 0 ? Text::YELLOW : Text::WHITE, category_names[i]};
        }
        page_label.change_text("Page 1 / 1");
        update_page_controls();
    }

    void UIMTS::update_screen(int16_t width, int16_t height)
    {
        position = {static_cast<int16_t>(std::max(0, (width - 800) / 2)),
                    static_cast<int16_t>(std::max(0, (height - 600) / 2))};
    }

    void UIMTS::draw(float alpha) const
    {
        ColorBox(Constants::viewwidth(), Constants::viewheight(), Geometry::BLACK, 1).draw({0, 0});
        background.draw(position);
        Text(Text::A18M, Text::CENTER, Text::BLACK, "Maple Trading System").draw(position + Point<int16_t>(400, 34));
        for (size_t i = 0; i < views.size(); ++i)
            views[i].draw(position + Point<int16_t>(235 + i * 160, 50));
        if (active_view == 0)
            for (size_t i = 0; i < categories.size(); ++i)
                categories[i].draw(position + Point<int16_t>(98 + i * 120, 86));
        Text(Text::A11B, Text::LEFT, Text::BLACK, "Item", 0, false).draw(position + Point<int16_t>(70, 130));
        Text(Text::A11B, Text::LEFT, Text::BLACK, "Seller / Price / Fee", 0, false).draw(position + Point<int16_t>(360, 130));
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto y = static_cast<int16_t>(156 + i * 23);
            if (!rows[i].name.empty())
            {
                rows[i].icon.draw({position + Point<int16_t>(42, y - 1), 0.65f, 0.65f});
                rows[i].name.draw(position + Point<int16_t>(78, y));
                rows[i].details.draw(position + Point<int16_t>(360, y));
            }
        }
        Text(Text::A11M, Text::LEFT, Text::WHITE, "< Prev").draw(position + Point<int16_t>(315, 554));
        Text(Text::A11M, Text::LEFT, Text::WHITE, "Next >").draw(position + Point<int16_t>(426, 554));
        page_label.draw(position + Point<int16_t>(400, 554));
        balance_label.draw(position + Point<int16_t>(670, 34));
        Text(Text::A11M, Text::LEFT, Text::WHITE, "Exit").draw(position + Point<int16_t>(714, 554));
        status.draw(position + Point<int16_t>(42, 584));
        UIElement::draw(alpha);
    }

    void UIMTS::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            CashShop::get().leave();
    }

    void UIMTS::update()
    {
        UIElement::update();
        status.change_text(CashShop::get().message);
        update_page_controls();
    }

    void UIMTS::set_page(MTSPage page)
    {
        current_page = std::move(page);
        active_view = 0;
        for (size_t i = 0; i < views.size(); ++i)
            views[i].change_color(i == 0 ? Text::YELLOW : Text::WHITE);
        for (size_t i = 0; i < categories.size(); ++i)
            categories[i].change_color(static_cast<int32_t>(i) == current_page.category ? Text::YELLOW : Text::WHITE);
        rebuild_rows();
        update_page_controls();
    }

    void UIMTS::set_transfers(std::vector<MTSListing> listings)
    {
        transfers = std::move(listings);
        if (active_view == 1) rebuild_rows();
    }

    void UIMTS::set_sales(std::vector<MTSListing> listings)
    {
        sales = std::move(listings);
        if (active_view == 2) rebuild_rows();
    }

    void UIMTS::set_wallet(int32_t nx, int32_t maple_points)
    {
        balance_label.change_text("NX " + std::to_string(nx) + "   Maple Points " + std::to_string(maple_points));
    }

    void UIMTS::rebuild_rows()
    {
        const auto& listings = active_listings();
        const size_t offset = active_view == 0 ? 0 : static_cast<size_t>(local_page) * rows.size();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            rows[i] = {};
            if (i + offset >= listings.size())
                continue;
            const auto& listing = listings[i + offset];
            const auto& item = ItemData::get(listing.item_id);
            rows[i].icon = item.get_icon(false);
            const std::string name = item.is_valid() ? item.get_name() : "Item " + std::to_string(listing.item_id);
            rows[i].name = {Text::A11M, Text::LEFT, Text::BLACK, name, 260, false};
            rows[i].details = {Text::A11M, Text::LEFT, Text::BLACK,
                listing.seller + "  |  x" + std::to_string(listing.quantity) + "  |  " +
                std::to_string(listing.price) + " NX  |  fee " + std::to_string(listing.fee), 385, false};
        }
    }

    void UIMTS::request_page()
    {
        if (CashShop::get().state == CashShop::State::OPEN)
            MTSBrowsePacket(1, current_page.category, current_page.page).dispatch();
    }

    void UIMTS::update_page_controls()
    {
        const bool open = CashShop::get().state == CashShop::State::OPEN;
        const int32_t current = active_view == 0 ? current_page.page : local_page;
        int32_t pages = active_view == 0 ? current_page.total / 16 + (current_page.total % 16 != 0) :
            static_cast<int32_t>((active_listings().size() + 15) / 16);
        pages = std::clamp(pages, 1, 626);
        buttons[EXIT]->set_active(open);
        buttons[PREVIOUS]->set_active(open && current > 0);
        buttons[NEXT]->set_active(open && current + 1 < pages);
        for (uint16_t i = 0; i < views.size(); ++i)
            buttons[VIEW_FIRST + i]->set_active(open);
        for (uint16_t i = 0; i < categories.size(); ++i)
            buttons[CATEGORY_FIRST + i]->set_active(open && active_view == 0);
        page_label.change_text("Page " + std::to_string(current + 1) + " / " + std::to_string(pages));
    }

    const std::vector<MTSListing>& UIMTS::active_listings() const
    {
        if (active_view == 1) return transfers;
        if (active_view == 2) return sales;
        return current_page.listings;
    }

    Button::State UIMTS::button_pressed(uint16_t id)
    {
        if (id == EXIT)
        {
            CashShop::get().leave();
        }
        else if (id == PREVIOUS || id == NEXT)
        {
            if (active_view == 0)
            {
                current_page.page += id == NEXT ? 1 : -1;
                request_page();
            }
            else
            {
                local_page += id == NEXT ? 1 : -1;
                rebuild_rows();
            }
        }
        else if (id >= VIEW_FIRST && id < VIEW_FIRST + views.size())
        {
            active_view = id - VIEW_FIRST;
            local_page = 0;
            for (size_t i = 0; i < views.size(); ++i)
                views[i].change_color(i == active_view ? Text::YELLOW : Text::WHITE);
            rebuild_rows();
        }
        else if (id >= CATEGORY_FIRST && id < CATEGORY_FIRST + categories.size())
        {
            active_view = 0;
            current_page.category = id - CATEGORY_FIRST;
            current_page.page = 0;
            for (size_t i = 0; i < views.size(); ++i)
                views[i].change_color(i == 0 ? Text::YELLOW : Text::WHITE);
            request_page();
        }
        rebuild_rows();
        update_page_controls();
        return Button::NORMAL;
    }
}
