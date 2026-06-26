#pragma once
#include "log.h"
#include "socket_utils.h"
#include "common.h"
#include "order.h"




// TODO: decide on an appropriate buffer size (batch_size) --> buffer size determines how much data can be in flight, also MSG_SIZE
// problem is bdp (figure out round trip and link access), consider tuning if dropping packets
// Implement get_timestamp_us


static constexpr size_t BATCH_SIZE = 1024;
static constexpr size_t BATCH_TIMEOUT_US = 10; 
static constexpr size_t MSG_SIZE = 64; 

class TCPSocket {
public:
    TCPSocket() {
        send_buffer = new char[BATCH_SIZE];
        recv_buffer = new char[BATCH_SIZE];
    }

    ~TCPSocket() {
        close(fd);
        delete[] send_buffer;
        delete[] recv_buffer;
    }

    // Create a socket, create_socket handles non-blocking, nagles algorithm, and connect/bind for client/server
    void connect(const std::string& t_ip, const std::string& iface, int port, bool is_server) {
        fd = -1;
        fd = create_socket(t_ip, iface, port, is_server, true);
        if (fd == -1) {
            LOG("Could not make a TCP socket");
        }
        recv_disconnected = false;
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

    // either constantly poll for new messages (clients) or use epoll for servers, should be in a loop
    // copy data from kernel buffer into recv buffer
    // handle framing (bytes are sent, can send half a message, need ot handle that)
    void recv_message() {
        // copy data from kernel buffer into recv buffer, use write pointer to ensure no overwrites
        ssize_t bytes_received = recv(fd, recv_buffer + recv_buf_pos, sizeof(recv_buffer - 1), MSG_DONTWAIT);
        if (bytes_received > 0) {
            recv_buf_pos += bytes_received;
            consume_messages();
        } else if (bytes_received == 0) {
            LOG("recv_message: Connection closed between TCP sockets");
            recv_disconnected = true;
        } else {
            LOG("recv_message: Could not receive bytes");
            exit(EXIT_FAILURE);
        }
    }
    
    // delete copy/move constructor and assignment operations
    TCPSocket(const TCPSocket& other) = delete;
    TCPSocket(TCPSocket&& other) = delete;
    TCPSocket& operator=(const TCPSocket& other) = delete;
    TCPSocket& operator=(TCPSocket&& other) = delete;

    // let class that is wrapped handle processing the message
    std::function<void(char*, size_t)> process_message;

private:
    struct Header {
        uint32_t payload_len; // encode the payload len in this to know how many bytes required for this message
    };

    // flush out the buffer (send it) and reset the buffer
    void flush_buffer() {
        if (send_buf_pos == 0) return;
        send(fd, send_buffer, sizeof(send_buffer), 0);
        first_message = 0;
        send_buf_pos = 0;
    }

    // consume messages until there isn't a full one
    void consume_messages() {
        while (true) {
            if (recv_buf_pos < sizeof(Header)) {
                return;  // incomplete header
            }
            
            // read the header info, check if we have a complete message
            Header hdr;
            memcpy(&hdr, recv_buffer , sizeof(Header));
            size_t total_msg_size = sizeof(Header) + hdr.payload_len;
            if (recv_buf_pos < total_msg_size) {
                return; // incomplete message
            }

            // process message use callback, wrapped class will implement how to process it
            process_message(recv_buffer, total_msg_size);

            // after handing message, move remaining bytes to the front
            // handles half completed messages, by appending if need be
            memmove(recv_buffer, recv_buffer + total_msg_size, recv_buf_pos - total_msg_size);
            recv_buf_pos -= total_msg_size;
        }
    }
    
    // member variables
    int fd;
    
    // send function variables
    char* send_buffer = nullptr;
    size_t send_buf_pos = 0;
    size_t first_message = 0;  
    
    // recv function variables
    char* recv_buffer = nullptr;
    size_t recv_buf_pos = 0;
    bool recv_disconnected = true;

};


class TCPServer {
    TCPServer() {
        // create an epollfd

    }
private:
    int epfd;
};

