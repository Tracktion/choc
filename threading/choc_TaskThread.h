//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_TASKTHREAD_HEADER_INCLUDED
#define CHOC_TASKTHREAD_HEADER_INCLUDED

#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include "../platform/choc_Assert.h"

namespace choc::threading
{

//==============================================================================
/**
    Manages a thread which will invoke a given callback function, either
    repeatedly at a given time-interval, or in response to another
    thread calling TaskThread::trigger().

    It's quite common to need a thread which performs a background task repeatedly,
    with a period of sleeping in between. But the standard library makes it quite a
    palaver to do this in a way that lets you interrupt the sleep to either retrigger
    the task immediately, or to destroy the thread. This class makes that job
    nice and easy.
*/
struct TaskThread
{
    TaskThread();
    ~TaskThread();

    /// Starts the thread running with a given interval and task function.
    /// If repeatIntervalMillisecs == 0, the task function will be invoked
    /// only when trigger() is called. If the interval is > 0, then
    /// whenever this number of milliseconds have passed without it being
    /// triggered, it'll be automatically invoked again.
    /// If the thread is already running when this it called, it will
    /// first be stopped.
    template<typename Rep, typename Period>
    void start (std::chrono::duration<Rep, Period> repeatInterval,
                std::function<void()> task);

    /// Starts the thread running with a given interval and task function.
    /// If repeatInterval has no length, the task function will be invoked
    /// only when trigger() is called. If the interval is > 0, then
    /// whenever this time has passed without it being triggered, it'll be
    /// automatically invoked again.
    /// If the thread is already running when this it called, it will
    /// first be stopped.
    void start (uint32_t repeatIntervalMillisecs,
                std::function<void()> task);

    /// Stops the thread, waiting for it to finish. This may involve
    /// waiting for the callback function to complete if it's currently
    /// in progress.
    void stop();

    /// This causes the task to be invoked as soon as the thread
    /// is free to do so. Calling it multiple times may result in
    /// only one call to the task being invoked.
    void trigger();

private:
    //==============================================================================
    std::function<void()> taskFunction;
    std::thread thread;
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    std::atomic<bool> threadShouldExit { false };
    uint32_t interval = 0;

    void wait();
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline TaskThread::TaskThread() = default;
inline TaskThread::~TaskThread() { stop(); }

inline void TaskThread::start (uint32_t repeatIntervalMillisecs, std::function<void()> f)
{
    CHOC_ASSERT (f != nullptr);

    stop();
    taskFunction = std::move (f);
    interval = repeatIntervalMillisecs;
    threadShouldExit = false;
    flag.test_and_set (std::memory_order_acquire);

    thread = std::thread ([this]
    {
        wait();

        while (! threadShouldExit)
        {
            taskFunction();
            wait();
        }
    });
}

template<typename Rep, typename Period>
void TaskThread::start (std::chrono::duration<Rep, Period> i, std::function<void()> task)
{
    start (static_cast<uint32_t> (std::chrono::duration_cast<std::chrono::milliseconds> (i).count()),
           std::move (task));
}

inline void TaskThread::stop()
{
    if (thread.joinable())
    {
        threadShouldExit = true;
        trigger();
        thread.join();
    }
}

inline void TaskThread::trigger()
{
    flag.clear (std::memory_order_release);
}

inline void TaskThread::wait()
{
    if (! flag.test_and_set (std::memory_order_acquire))
        return;

    uint32_t numTries = 0;

    auto yieldOrSleep = [&numTries]
    {
        static constexpr uint32_t numTriesBeforeSleeping = 1000;
        static constexpr uint32_t sleepDuration = 5;

        if (numTries == numTriesBeforeSleeping)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (sleepDuration));
        }
        else
        {
            std::this_thread::yield();
            ++numTries;
        }
    };

    if (interval != 0)
    {
        auto timeout = std::chrono::high_resolution_clock::now()
                        + std::chrono::milliseconds (interval);

        for (;;)
        {
            yieldOrSleep();

            if (! flag.test_and_set (std::memory_order_acquire))
                return;

            if (std::chrono::high_resolution_clock::now() >= timeout)
                return;
        }
    }
    else
    {
        for (;;)
        {
            yieldOrSleep();

            if (! flag.test_and_set (std::memory_order_acquire))
                return;
        }
    }
}


} // namespace choc::threading

#endif // CHOC_TASKTHREAD_HEADER_INCLUDED
