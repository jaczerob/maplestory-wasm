#pragma once

#include "../UIElement.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../../Net/Handlers/Helpers/MTSParser.h"

#include <array>
#include <vector>

namespace jrc
{
    class UIMTS : public UIElement
    {
    public:
        static constexpr Type TYPE = CASHSHOP;
        static constexpr bool FOCUSED = true;
        static constexpr bool TOGGLED = false;

        UIMTS();
        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t width, int16_t height) override;
        void send_key(int32_t key, bool pressed, bool escape) override;
        void set_page(MTSPage page);
        void set_transfers(std::vector<MTSListing> listings);
        void set_sales(std::vector<MTSListing> listings);
        void set_wallet(int32_t nx, int32_t maple_points);

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        struct Row
        {
            Texture icon;
            Text name;
            Text details;
        };

        void request_page();
        void rebuild_rows();
        void update_page_controls();
        const std::vector<MTSListing>& active_listings() const;

        Texture background;
        std::array<Row, 16> rows;
        std::array<Text, 6> categories;
        std::array<Text, 3> views;
        Text page_label;
        Text balance_label;
        Text status;
        MTSPage current_page;
        int16_t local_page = 0;
        uint8_t active_view = 0;
        std::vector<MTSListing> transfers;
        std::vector<MTSListing> sales;
    };
}
