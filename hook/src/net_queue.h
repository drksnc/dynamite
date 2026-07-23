#pragma once
#include <mutex>
#include <queue>
#include "packet.h"

class NetQueue {
    std::mutex m_mutex;
    std::queue<Packet *> mainQueue;

  public:
    NetQueue() = default;
    Packet *Create(void *ptr, const size_t size);
    Packet *Retrieve();
    Packet *GetFirst();
};
