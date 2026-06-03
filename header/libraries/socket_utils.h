
#pragma once
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <string>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <cctype>

#include "log.h"


constexpr u_int16_t MAX_LENGTH = 1024;
std::string get_interface(const std::string& input) {
    // should convert a string into a interface type
    char buf[MAX_LENGTH];
    ifaddrs *ifap;
    // getifaddrs creates a linked list of ifaddrs of network interfaces in system
    // ifaddr is a struct that has interface name, ip address, network mask, and broadcast system
    if (getifaddrs(&ifap) == -1){
        LOG("getifaddrs: could not get network IP addresses");
    }
    // use getifaddrs, traverse through linked structure until you find it (use strmcp)
    for (ifaddrs* ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {

    }   

    free(ifap);
    return buf;
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

// Mac doens't expose hardware level timing, so either run on Linux VM or use software level timing
auto set_timestamp(int fd) {
    int optval = 1;
    // SO_TIMESTAMP flag allows the kernel to record when the packet enters the socket, use SOL_SOCKET for SO*_ options
    // You retrieve it via recvmsg()
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1);
}


auto create_socket(const std::string& t_ip, const std::string& iface, bool is_tcp, bool is_server, int port) {
    const auto ip = t_ip.empty() ? get_interface(iface) : t_ip;
    
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    addrinfo* result = nullptr;

    
    hints.ai_flags |= is_server ? AI_PASSIVE : 0;
    hints.ai_flags |= AI_NUMERICSERV;
    if (std::isdigit(ip[0])) {
        hints.ai_flags |= AI_NUMERICHOST;
    }
    hints.ai_family = AF_INET;
    hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_protocol = is_tcp ? IPPROTO_TCP : IPPROTO_UDP;

    if (int rc = getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
        fprintf(stderr, "create_socket: Error in getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    // Loop until we can find a suitable scoket
    int fd = -1;
    for (addrinfo* ai = result; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == -1) continue;
        
    }

    if (fd == -1) {
        LOG("create_socket: Could not find any compatiable addresses to connect a socket to");
        return -1;
    }

}

