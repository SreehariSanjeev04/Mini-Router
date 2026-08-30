#pragma once

#include "arp/ArpCache.h"
#include "arp/ArpHandler.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"

#include <memory>
#include <utility>
#include <vector>

class Router
{
public:
    Router();

    void run();

private:
    std::vector<std::unique_ptr<RawSocket>> rawSockets_;
    InterfaceManager interfaces_;
    ArpCache arpCache_;
    ArpHandler arpHandler_;
};