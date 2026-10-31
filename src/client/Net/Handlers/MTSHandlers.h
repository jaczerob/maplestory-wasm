#pragma once

#include "../PacketHandler.h"

namespace jrc
{
    class SetMTSHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class MTSOperationHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class MTSWalletHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };
}
