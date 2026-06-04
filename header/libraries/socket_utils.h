
#pragma once

// libraries
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <string>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cctype>



// files
#include "log.h"


// globals
constexpr u_int16_t MAX_LENGTH = 1024;
constexpr int MAX_TCP_CONNECTIONS = 1024;


// TODO: finish socket utilities, tcp_socket class
//       create_socket is not done rework it, get_interface is not done, setup multicast udp not done

// functions

// create the socket based on whether its a client or server, and if its tcp or udp
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
    addrinfo* ai;
    for (ai = result; ai; ai = ai->ai_next) {
        if (fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol) != -1) {
            break;
        }
    }

    if (fd == -1) {
        LOG("create_socket: Could not find any compatiable addresses to connect a socket to");
        return -1;
    }

    // TODO: does this need to be in loop, should we do this for each address that works, or just 1 of them
    // set it to non-blocking and disable nagles on tcp
    if (!would_block) {
        // set non-blocking regardless of tcp or UDP
        if (!set_nonblocking(fd)) {
            LOG("create_socket: Could not set fd as non-blocking");
            return -1;
        }
        if (is_tcp) {
            if (!disable_nagles(fd)) {
                LOG("create_socket: Could not disable nagles on a TCP socket");
                return -1;
            }
        }
    }

    // decide whether or not to connect, or bind and listen 
    int one = 1;
    if (is_server) {
        // bind this socket to the address and wait for incoming requests
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) & bind(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
            LOG("create_socket: Could not bind socket");
            return -1;
        }
        if (listen(fd, MAX_TCP_CONNECTIONS) == -1) {
            LOG("create_socket: Could not listen at this socket");
            return -1;
        }
    } else {
        // client socket, connect it
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
            LOG("create_socket: Could not connect socket");
            return -1;
        }
    }
    // maybe implement time to live functionality

    return fd;
}


// helper functions
// get the interface from a string
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
    //TODO:
    for (ifaddrs* ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {

    }   

    free(ifap);
    return buf;
}

// set socket to be non-blocking
auto set_nonblocking(int fd) {
    // fcntl manipulates flags of a file, F_GETFL returns file flags
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG("set_nonblocking: Unable to set " << fd << " as nonblocking.");
        return false;
    }
    // TODO: Check if socket is already nonblocking
    // F_SETFL sets a nonblocking flag
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1);
}

// disable nagles for TCP sockets
auto disable_nagles(int fd) {
    // optval = 1 means modify, 0 means disable
    int optval = 1;
    // setsockopt sets option flag (TCP_NODELAY) for fd
    if ((setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1)) {
        LOG("disable_nagles: Unable to disable nagle's algorithm on " << fd);
        return false;
    }
}

// Check if socket operation would block
auto would_block() {
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

// send data over multicast udp to all subscribed clients
int setup_multicast_client(const char* group, int fd) { 
    // pass in a group to set up multicast udp to
    // clients will subscribe to this ip address and receive data sent over this group ip address

    // make sure fd is AF_NET and SOCK_DGRAM

    // reuse address is process crashed? does this need to be here or elsewhere
    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        LOG("setup_multicast: Could not set sock");
    }

    ip_mreq mreq;
    // make multi address of groups address
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    // only one network card, let kernel choose (0.0.0.0)
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

    // joins a multicast group; for clients
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
        LOG("setup_multicast: Could not add membership to IP group");
    }

}