#pragma once
#include "log.h"
#include "socket_utils.h"

// TODO: decide on an appropriate buffer size --> buffer size determines how much data can be in flight, problem is bdp (figure out round trip and link access), consider tuning if dropping packets
static constexpr size_t BUFFER_SIZE = 1024;
struct TCPSocket {
    // needs a send and receive buffer
    // need a fd to match to
    // need to know if send and receive sockets are both connected
    size_t next_rec_idx = 0;
    size_t next_send_idx = 0;
    char* send_buf = nullptr;
    char* rec_buf = nullptr;
    int fd = -1;
    bool send_connected = false;
    bool rec_connected = false;


    // Manually allocation should be fine since this isn't hot path
    explicit TCPSocket(int fd_) : fd(fd_), send_buf(new char[BUFFER_SIZE]), rec_buf(new char[BUFFER_SIZE]) {};

    TCPSocket(const TCPSocket& other) = delete;
    TCPSocket& operator=(const TCPSocket& other) = delete;

    TCPSocket(TCPSocket&& other) = delete;
    TCPSocket& operator=(TCPSocket&& other) = delete;

    // Write a function that allows connections
    ~TCPSocket() {
        close(fd); fd = -1;
        delete[] send_buf; send_buf = nullptr;
        delete[] rec_buf; rec_buf = nullptr;
    }

   
};