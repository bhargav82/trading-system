#pragma once
#include <iostream>
#include <mutex>

std::mutex m;
#define LOG(x) do { \
    m.lock(); \
    std::cout << x << std::endl; \
    m.unlock(); \
    } while(0)
