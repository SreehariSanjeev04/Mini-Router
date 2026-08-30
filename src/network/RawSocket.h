#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>

class RawSocket
{
public:
    RawSocket(int protocol, int interfaceIndex);
    ~RawSocket();

    RawSocket(const RawSocket&) = delete;
    RawSocket& operator=(const RawSocket&) = delete;

    int descriptor() const;
    int interfaceIndex() const;
    ssize_t receive(void* buffer, size_t length);
    ssize_t send(const void* buffer, size_t length);

private:
    int fd_;
    int protocol_;
    int interfaceIndex_;
};