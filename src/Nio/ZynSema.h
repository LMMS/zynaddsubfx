#ifndef ZYNSEMA_H
#define ZYNSEMA_H

#if defined __APPLE__ || defined WIN32

#include "ThreadShims.h"

class ZynSema
{
public:
    ZynSema (void) : _count (0)
    {
    }

    int init (int, int v)
    {
        _count = v;
        return 0;
    }

    int post (void)
    {
        _mutex.lock();
        if (++_count == 1) _cond.notify_one();
        _mutex.unlock();
        return 0;
    }

    int wait (void)
    {
        auto lock = std::unique_lock{_mutex};
        while (_count < 1) _cond.wait(lock);
        --_count;
        return 0;
    }

    int trywait (void)
    {
        if (!_mutex.try_lock()) return -1;
        if (_count < 1)
        {
            _mutex.unlock();
            return -1;
        }
        --_count;
        _mutex.unlock();
        return 0;
    }

    int getvalue (void) const
    {
        return _count;
    }


private:
    int              _count;
    std::mutex _mutex;
    std::condition_variable _cond;
};

#else // POSIX sempahore

#include <semaphore.h>

class ZynSema
{
public:
    ZynSema (void)
    {
    }
    ~ZynSema (void)
    {
        sem_destroy (&_sema);
    }
    int init (int s, int v)
    {
        return sem_init (&_sema, s, v);
    }
    int post (void)
    {
        return sem_post (&_sema);
    }
    int wait (void)
    {
        return sem_wait (&_sema);
    }
    int trywait (void)
    {
        return sem_trywait (&_sema);
    }
    int getvalue(void)
    {
        int v = 0;
        sem_getvalue(&_sema, &v);
        return v;
    }

private:
    sem_t _sema;
};

#endif // POSIX semapore

#endif // ZYNSEMA_H
