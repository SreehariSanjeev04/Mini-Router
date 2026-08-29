#include "RawSocket.h"

int Socket::createRawSocket(int protocol) {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
    if (sockfd < 0) {
        return -1;
    }
    return sockfd;
}