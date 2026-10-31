#include "MTSHandlers.h"

#include "Helpers/MTSParser.h"
#include "SetfieldHandlers.h"
#include "../PacketError.h"
#include "../../Gameplay/CashShop.h"
#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIMTS.h"
#include "../../Audio/Audio.h"

namespace jrc
{
    void SetMTSHandler::handle(InPacket& recv) const
    {
        auto& shop = CashShop::get();
        recv.skip(9); // Character-information mask and reserved byte.
        SetfieldHandler().parse_character(recv);
        shop.account_name = MTSParser::entry_account(recv);

        shop.trading = true;
        shop.opened();
        Stage::get().clear();
        UI::get().change_state(UI::CASHSHOP);
        UI::get().enable();
        Music("BgmUI.img/CashShop").play();
    }

    void MTSOperationHandler::handle(InPacket& recv) const
    {
        auto& shop = CashShop::get();
        if (!shop.trading || (shop.state != CashShop::State::OPEN && shop.state != CashShop::State::EXITING))
        {
            recv.skip(recv.length());
            return;
        }
        auto mts = UI::get().get_element<UIMTS>();
        switch (static_cast<uint8_t>(recv.read_byte()))
        {
        case 0x15:
            if (mts) mts->set_page(MTSParser::page(recv));
            else MTSParser::page(recv);
            CashShop::get().notify("MTS listings loaded.");
            break;
        case 0x21:
        {
            auto listings = MTSParser::listings(recv);
            const auto footer = static_cast<uint8_t>(recv.read_byte());
            if (footer != static_cast<uint8_t>(0xD0 + listings.size()) ||
                recv.read_byte() != -1 || recv.read_byte() != -1 || recv.read_byte() != -1 || recv.read_byte() != 0)
                throw PacketError("Invalid MTS transfer listing footer");
            if (recv.available()) throw PacketError("Unexpected MTS transfer listing data");
            if (mts) mts->set_transfers(std::move(listings));
            break;
        }
        case 0x23:
        {
            auto listings = MTSParser::listings(recv);
            if (recv.available() && (listings.size() != 0 || recv.read_int() != 0 || recv.available()))
                throw PacketError("Invalid MTS sales listing footer");
            if (mts) mts->set_sales(std::move(listings));
            break;
        }
        case 0x3D:
            recv.read_int();
            recv.read_int();
            break;
        case 0x33:
            CashShop::get().notify("MTS purchase complete.");
            break;
        case 0x34:
            recv.read_byte();
            CashShop::get().notify("MTS purchase failed. Check the listing and your NX balance.");
            break;
        case 0x1D:
            CashShop::get().notify("MTS listing updated.");
            break;
        case 0x27:
            recv.read_int();
            recv.read_int();
            CashShop::get().notify("MTS transfer updated.");
            break;
        default:
            recv.skip(recv.length());
            break;
        }
    }

    void MTSWalletHandler::handle(InPacket& recv) const
    {
        const auto nx = recv.read_int();
        const auto maple_points = recv.read_int();
        if (recv.available()) throw PacketError("Unexpected MTS wallet data");
        const auto& shop = CashShop::get();
        if (shop.trading && (shop.state == CashShop::State::OPEN || shop.state == CashShop::State::EXITING))
        {
            if (auto mts = UI::get().get_element<UIMTS>())
                mts->set_wallet(nx, maple_points);
        }
    }
}
