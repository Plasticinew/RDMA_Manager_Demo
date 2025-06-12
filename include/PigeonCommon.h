
#pragma once

#include <cassert>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <thread>
#include <map>

#include <netdb.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "Msg.h"

#define TIME_NOW (std::chrono::high_resolution_clock::now())
#define TIME_DURATION_US(start, end) (std::chrono::duration_cast<std::chrono::microseconds>((end) - (start)).count())

// const int RESOLVE_TIMEOUT_MS = 5000;
// const int RDMA_TIMEOUT_US = 100000000;  // 10s

struct PigeonDevice {
    std::string name;
    std::string ip;
    std::string netname;
    int port;
};

enum PigeonStatus {
    PIGEON_STATUS_INIT,
    PIGEON_STATUS_CONNECTED,
    PIGEON_STATUS_ACCEPTED,
    PIGEON_STATUS_ERROR
};

inline void pigeon_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

inline void pigeon_swap(int& a, int& b) { int temp = a; a = b; b = temp; }