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
        send_buffer = new char[BATCH_SIZE];
        recv_buffer = new char[BATCH_SIZE];
        
    }

    void connect(const std::string& t_ip, const std::string& iface, int port) {
        fd = -1;
        fd = create_socket(t_ip, iface, true, false, port);
        if (fd == -1) {
            LOG("Could not make a TCP socket");
        }
    }

    // Batch messages together -> less system calls
    void send_order(const Order& order) {
        // keep track of first message sent
        if (send_buf_pos == 0) [[unlikely]] {
            first_message = get_timestamp_us();
        }
        
        // add this order to the batch buffer
        size_t order_sz = sizeof(Order);
        memcpy(send_buffer + send_buf_pos, &order, sizeof(order));
        send_buf_pos += order_sz;
        size_t curr_message = get_timestamp_us();
        
        // if the batch has overflowed or we have waited long enough -> ready to send
        if (curr_message - first_message > BATCH_TIMEOUT_US || send_buf_pos >= BATCH_SIZE - 100) {
            flush_buffer();
        }
    }
    
    TCPSocket() = delete;
    TCPSocket(const TCPSocket& other) = delete;
    TCPSocket(TCPSocket&& other) = delete;
    TCPSocket& operator=(const TCPSocket& other) = delete;
    TCPSocket& operator=(TCPSocket&& other) = delete;

private:
    void flush_buffer() {
        if (send_buf_pos == 0) return;
        send(fd, send_buffer, sizeof(send_buffer), 0);
        first_message = 0;
        send_buf_pos = 0;
    }


    int fd;
    char* send_buffer = nullptr;
    size_t send_buf_pos = 0;
    size_t first_message = 0;  
    
    char* recv_buffer = nullptr;
    size_t recv_buf_pos = 0;

};

