#include "net_queue.h"

Packet *NetQueue::Create(void *ptr, const size_t size) 
{
    std::lock_guard<std::mutex> lock(m_mutex);

    Packet *p = new Packet();
    p->WriteBytes(ptr, size);

    mainQueue.push(p);

    return mainQueue.back();
}

Packet *NetQueue::Retrieve() 
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (mainQueue.empty())
        return nullptr;

    auto *p = mainQueue.front();
    mainQueue.pop();
    return p;
}

Packet *NetQueue::GetFirst() 
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (mainQueue.empty())
        return nullptr;

    return mainQueue.front();
}