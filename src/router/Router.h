#pragma once

#include "arp/ArpCache.h"
#include "arp/ArpHandler.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"

class Router
{
public:
    Router();

    void run();

private:
    RawSocket rawSocket_;
    InterfaceManager interfaces_;
    ArpCache arpCache_;
    ArpHandler arpHandler_;
    bool enableProxyArp_ = true;
};