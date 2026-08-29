#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>

class RawSocket
{
public:
    explicit RawSocket(int protocol);
    ~RawSocket();

    RawSocket(const RawSocket&) = delete;
    RawSocket& operator=(const RawSocket&) = delete;

    int descriptor() const;
    ssize_t receive(void* buffer, size_t length);

    ssize_t send(const void* buffer, size_t length);

private:
    int fd_;
    int lastInterfaceIndex_;
};