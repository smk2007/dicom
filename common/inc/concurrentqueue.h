#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

namespace Concurrency
{

template <typename T>
class ConcurrentQueue
{
    std::vector<T> m_queue;
    unsigned m_readPos = 0;
    unsigned m_writePos = 0;
    std::mutex m_mutex;
    std::condition_variable m_empty;
    std::condition_variable m_full;

private:

    unsigned NextRead()
    {
        return (m_readPos + 1) % m_queue.size();
    }

    void IncrementRead()
    {
        m_readPos = (m_readPos + 1) % m_queue.size();
    }

    unsigned NextWrite()
    {
        return (m_writePos + 1) % m_queue.size();
    }

    void IncrementWrite()
    {
        m_writePos = (m_writePos + 1) % m_queue.size();
    }

public:
    ConcurrentQueue(unsigned size) :
        m_queue(size + 1)
    {
    }

    HRESULT Enqueue(T&& obj)
    {
        std::unique_lock<mutex> lock(m_mutex);
        m_full.wait(lock, [&](){ return m_readPos != NextWrite(); });

        m_queue[m_writePos] = std::move(obj);

        IncrementWrite();

        m_empty.notify_all();

        return S_OK;
    }

    HRESULT Dequeue(T* pObj)
    {
        RETURN_HR_IF_NULL(E_POINTER, pObj);
        std::unique_lock<mutex> lock(m_mutex);
        m_empty.wait(lock, [&](){ return m_readPos != m_writePos; });

        *pObj = std::move(m_queue.at(m_write));

        IncrementRead();

        m_full.notify_all();

        return S_OK;
    }

    bool IsEmpty() const
    {
        return m_readPos == m_writePos;
    }
};

} // namespace Concurrent