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

#ifndef CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED

#include "choc_javascript.h"
#include "../gui/choc_MessageLoop.h"


namespace choc::javascript
{

//==============================================================================
/** This function will bind some standard setInterval/setTimeout functions to
    a javascript engine.

    Just call registerTimerFunctions() on your choc::javascript::Context object
    and it will bind the appropriate functions for you to use.

    The timer implementation uses choc::messageloop::Timer, and the callbacks will
    be invoked on the message thread. So to work correctly, your app must be running
    a message loop, and you must be careful not to cause race conditions by calling
    into the same javascript context from other threads.

    The following functions are added, which should behave as you'd expect their
    standard javascript counterparts to work:

        setInterval (callback, milliseconds) // returns the new timer ID
        setTimeout (callback, milliseconds)  // returns the new timer ID
        clearInterval (timerID)
*/
void registerTimerFunctions (choc::javascript::Context&);





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

inline void registerTimerFunctions (Context& context)
{
    struct State
    {
        choc::messageloop::Timer timer;
        int64_t lastCallback = 0;
    };

    context.registerFunction ("choc_setIntervalForNextTimerCallback",
                              [state = std::make_shared<State>(), &context] (ArgumentList args) mutable -> choc::value::Value
    {
        if (auto interval = args.get<int> (0); interval > 0)
        {
            state->timer = choc::messageloop::Timer (static_cast<uint32_t> (interval), [&]
            {
                auto now = std::chrono::steady_clock::now().time_since_epoch();
                auto millisecsNow = std::chrono::duration_cast<std::chrono::milliseconds> (now).count();
                auto millisSinceLastCall = state->lastCallback != 0 ? std::max (0, static_cast<int> (millisecsNow - state->lastCallback)) : 0;
                state->lastCallback = millisecsNow;

                context.invoke ("_choc_invokeTimers", millisSinceLastCall);

                return true;
            });
        }
        else
        {
            state->timer = {};
        }

        return {};
    });

    context.evaluate (R"(
var choc_activeTimers = [];
var choc_currentTimerInterval = -1;
var choc_nextTimerID = 1;

function _choc_addTimer (callback, milliseconds, interval)
{
    const timer = {
        remaining: milliseconds,
        interval: interval,
        callback: callback,
        timerID: choc_nextTimerID++
    };

    choc_activeTimers.push (timer);

    if (choc_currentTimerInterval < 0 || milliseconds < choc_currentTimerInterval)
    {
        choc_currentTimerInterval = milliseconds;
        choc_setIntervalForNextTimerCallback (milliseconds);
    }

    return timer.timerID;
}

function _choc_invokeTimers (millisecsElapsed)
{
    var next = -1;

    for (var i = choc_activeTimers.length; --i >= 0;)
    {
        var t = choc_activeTimers[i];

        if (t.removed)
        {
            choc_activeTimers.splice (i, 1);
        }
        else
        {
            t.remaining -= millisecsElapsed;

            if (t.remaining <= 0)
            {
                var timerID = t.timerID;

                t.callback();

                if (t.removed)
                    continue;

                if (t.interval <= 0)
                {
                    choc_activeTimers.splice (i, 1);
                }
                else
                {
                    t.remaining = t.interval;

                    if (t.remaining < 1)
                        t.remaining = 1;
                }
            }

            if (next < 0 || t.remaining < next)
                next = t.remaining;
        }
    }

    choc_currentTimerInterval = next;
    choc_setIntervalForNextTimerCallback (next);
}

function setInterval (callback, milliseconds)
{
    return _choc_addTimer (callback, milliseconds, milliseconds);
}

function setTimeout (callback, milliseconds)
{
    return _choc_addTimer (callback, milliseconds, 0);
}

function clearInterval (timerID)
{
    for (var i = 0; i < choc_activeTimers.length; ++i)
    {
        if (choc_activeTimers[i].timerID === timerID)
        {
            choc_activeTimers[i].interval = 0;
            choc_activeTimers[i].remaining = 0;
            choc_activeTimers[i].removed = true;
            break;
        }
    }
}
)");
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_TIMER_HEADER_INCLUDED
