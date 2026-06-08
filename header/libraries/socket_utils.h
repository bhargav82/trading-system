
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
#include <arpa/inet.h>

// files
#include "log.h"


// globals
constexpr u_int16_t MAX_LENGTH = 1024;
constexpr int MAX_TCP_CONNECTIONS = 1024;


// TODO: finish socket utilities, tcp_socket class
//       setup multicast udp not done

// functions

auto create_socket(const std::string& t_ip, const std::string& iface, int port, bool is_server, bool is_tcp) {
    const auto ip = t_ip.empty() ? get_interface_ip(iface) : t_ip;
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    addrinfo* result = nullptr;

    hints.ai_flags = is_server ? AI_PASSIVE : 0;
    hints.ai_flags |= AI_NUMERICSERV;
    if (std::isdigit(ip[0])) {
        hints.ai_flags |= AI_NUMERICHOST;
    }
    hints.ai_family = AF_INET;
    hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM; 
    hints.ai_protocol = is_tcp ? IPPROTO_TCP : IPPROTO_UDP;

    // resolves hostname and service name and builds linked list
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
    
    return fd;
}



// Converts a network interface name into IP address string
auto get_interface_ip(const std::string& iface) {
    // need to walk through the linked list of all network interfaces and finds the one matching the name and extracts IP
    // should convert a string into a interface type
    char buf[MAX_LENGTH];
    ifaddrs *ifap;
    // getifaddrs creates a linked list of ifaddrs of network interfaces in system
    // ifaddr is a struct that has interface name, ip address, network mask, and broadcast system
    if (getifaddrs(&ifap) == -1){
        LOG("getifaddrs: could not get network IP addresses");
        exit(EXIT_FAILURE);
    }
    // use getifaddrs, traverse through linked structure until you find it (use strmcp)
    
    for (ifaddrs* ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET && ifa->ifa_name == iface) {
            int s = getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(MAX_LENGTH), NULL, 0, NI_NUMERICHOST);
            if (s == -1) {
                LOG("getnameinfo: could not get IP from name");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }   

    free(ifap);
    return buf;
}

// set socket to be non-blocking
auto set_nonblocking(int fd) {
    // fcntl manipulates flags of a file, F_GETFL returns file flags
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG("set_nonblocking: could not get flags.");
        return false;
    }
    // TODO: Check if socket is already nonblocking
    // F_SETFL sets a nonblocking flag
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1);
}

// disable nagles for TCP sockets
auto disable_nagles(int fd) {
    int optval = 1;
    // setsockopt sets option flag (TCP_NODELAY) for fd
    if ((setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1)) {
        LOG("disable_nagles: Unable to disable nagle's algorithm on");
        return false;
    }
}

// Check if socket operation would block
auto would_block() {
    /* all operations that would block will (usually) return with EAGAIN
       (operation should be retried later); connect(2) will return
       EINPROGRESS error.  The user can then wait for various events via
       poll(2) or select(2).*/
    return (errno == EWOULDBLOCK || errno == EINPROGRESS);
}


// Mac doens't expose hardware level timing, so either run on Linux VM or use software level timing
auto set_timestamp(int fd) {
    int optval = 1;
    // SO_TIMESTAMP flag allows the kernel to record when the packet enters the socket, use SOL_SOCKET for SO*_ options
    // You retrieve it via recvmsg()
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void*>(&optval), sizeof(optval)) == -1);
}


// send data over multicast udp to all subscribed clients
int setup_multicast_client(const char* group, int fd, int port) { 
    // pass in a group to set up multicast udp to
    // clients will subscribe to this ip address and receive data sent over this group ip address

    // make sure fd is AF_NET and SOCK_DGRAM

    // reuse address is process crashed? does this need to be here or elsewhere
    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        LOG("setup_multicast: Could not set sock");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // must bind so this socket gets the data
    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG("setup_multicast: bind failed");
        return -1;
    }

    ip_mreq mreq;
    // make multi address of groups address
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    // only one network card, let kernel choose (0.0.0.0)
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

    // joins a multicast group; for clients
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
        LOG("setup_multicast: Could not add membership to IP group");
        return -1;
    }

    return 0; // success
}








