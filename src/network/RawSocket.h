#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace Socket {
    int createRawSocket(int protocol);
}


