#pragma once
#include "log.h"
#include "socket_utils.h"
#include "common.h"
#include "order.h"

// TODO: decide on an appropriate buffer size --> buffer size determines how much data can be in flight, 
// problem is bdp (figure out round trip and link access), consider tuning if dropping packets
// Implement get_timestamp_us

// how do we use this class, do we use this as a part of a TCP socket

// TCP socket needs to be able to send and receive data

static constexpr size_t BATCH_SIZE = 1024;
static constexpr size_t BATCH_TIMEOUT_US = 10; 


class TCPSocket {
public:
    explicit TCPSocket(const std::string& t_ip, const std::string& iface) {
        // pick a port
        fd = create_socket(t_ip, iface, true, false, 0);
        if (fd == -1) {
            LOG("Could not make a TCP socket");
        }
    }
    
    TCPSocket() = delete;
    TCPSocket(const TCPSocket& other) = delete;
    TCPSocket(TCPSocket&& other) = delete;
    TCPSocket& operator=(const TCPSocket& other) = delete;
    TCPSocket& operator=(TCPSocket&& other) = delete;

private:
    int fd;
    TCPSender sender;
    TCPReceiver receiver;
};





class TCPSender {
public:
    void send_order(const Order& order) {
        // keep track of first message sent
        if (buf_pos == 0) [[unlikely]] {
            first_message = get_timestamp_us();
        }
        
        // add this order to the batch buffer
        size_t order_sz = sizeof(Order);
        memcpy(buffer + buf_pos, &order, sizeof(order));
        buf_pos += order_sz;
        size_t curr_message = get_timestamp_us();
        
        // if the batch has overflowed or we have waited long enough -> ready to send
        if (curr_message - first_message > BATCH_TIMEOUT_US || buf_pos >= BATCH_SIZE - 100) {
            flush_buffer();
        }
    }

private:
    void flush_buffer() {
        if (buf_pos == 0) return;
        send(sock, buffer, sizeof(buffer), 0);
        first_message = 0;
        buf_pos = 0;
    }

    char buffer[BATCH_SIZE];
    size_t buf_pos = 0;
    size_t first_message = 0;
    int sock;
};

class TCPReceiver {
public:
private:
    // does this just poll 
    char buffer[BATCH_SIZE];
    int sock;
};