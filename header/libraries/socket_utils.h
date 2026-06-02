
#pragma once
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <string>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "log.h"


constexpr u_int16_t MAX_LENGTH = 1024;
auto get_interface(const std::string& input) {
    // should convert a string into a interface type
    char buf[MAX_LENGTH];
    struct ifaddrs *ifap;
    // getifaddrs creates a linked list of ifaddrs of network interfaces in system
    // ifaddr is a struct that has interface name, ip address, network mask, and broadcast system
    getifaddrs(&ifap);
    // use getifaddrs, traverse through linked structure until you find it (use strmcp)

    free(ifap);
}

// fcntl manipulates flags of a file
auto set_nonblocking(int fd) {
    // F_GETFL returns file flags
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG("set_nonblocking: Unable to set " << fd << " as nonblocking.");
        return false;
    }
    // TODO: Check if socket is already nonblocking
    // F_SETFL sets a nonblocking flag
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1);
}

auto disable_nagles(int fd) {
    // optval = 1 means modify, 0 means disable
    int optval = 1;
    // setsockopt sets option flag (TCP_NODELAY) for fd
    if ((setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1)) {
        LOG("disable_nagles: Unable to disable nagle's algorithm on " << fd);
        return false;
    }
}


auto would_block() {
    // Check if socket operation would block
    /* all operations that would block will (usually) return with EAGAIN
       (operation should be retried later); connect(2) will return
       EINPROGRESS error.  The user can then wait for various events via
       poll(2) or select(2).*/
    return (errno == EAGAIN || errno == EINPROGRESS);
}

auto set_timestamp(int fd) {
    int optval = 1;
    // SO_TIMESTAMP flag allows the kernel to record when the packet enters the socket
    // /You retrieve it via recvmsg()
    return (setsockopt(fd, IPPROTO_TCP, SO_TIMESTAMP, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1);
}
