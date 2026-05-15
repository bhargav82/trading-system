#pragma once
#include <iostream>
#include <mutex>

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

std::mutex m;
#if LOG_LEVEL > 0
#define LOG(x) do { \
    m.lock(); \
    std::cout << x << std::endl; \
    m.unlock(); \
    } while(0)
#else 
#define LOG(msg) do {} while(0)

#endif