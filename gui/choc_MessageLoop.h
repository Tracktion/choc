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

#ifndef CHOC_MESSAGELOOP_HEADER_INCLUDED
#define CHOC_MESSAGELOOP_HEADER_INCLUDED

#include <functional>
#include "../platform/choc_Platform.h"


//==============================================================================
/**
    This namespace provides some bare-minimum event loop and message dispatch
    functions.

    Note that on Linux this uses GTK, so to build it you'll need to:
       1. Install the libgtk-3-dev package.
       2. Link the gtk+3.0 library in your build.
          You might want to have a look inside choc/tests/CMakeLists.txt for
          an example of how to add this packages to your build without too
          much fuss.
*/
namespace choc::messageloop
{
    /// Synchronously runs the system message loop. You'll need a message
    /// loop to be running for the WebView to work correctly.
    void run();

    /// Posts a message to make the message loop exit and terminate the app.
    void stop();

    /// Posts a function be invoked asynchronously by the message thread.
    void postMessage (std::function<void()>&&);
}


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


#if CHOC_LINUX

#include "../platform/choc_DisableAllWarnings.h"
#include <gtk/gtk.h>
#include "../platform/choc_ReenableAllWarnings.h"

inline void choc::messageloop::run()   { gtk_main(); }
inline void choc::messageloop::stop()  { gtk_main_quit(); }

inline void choc::messageloop::postMessage (std::function<void()>&& fn)
{
    g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                     (GSourceFunc) ([](void* f) -> int
                     {
                         (*static_cast<std::function<void()>*>(f))();
                         return G_SOURCE_REMOVE;
                     }),
                     new std::function<void()> (std::move (fn)),
                     [] (void* f) { delete static_cast<std::function<void()>*>(f); });
}

//==============================================================================
#elif CHOC_APPLE

#include <objc/objc-runtime.h>
#include <dispatch/dispatch.h>

namespace choc::objc
{
    static inline id getClass (const char* s)              { return (id) objc_getClass (s); }
    static inline SEL getSelector (const char* s)          { return sel_registerName (s); }

    template <typename ReturnType, typename... Args>
    static ReturnType call (id target, const char* selector, Args... args)
    {
        return reinterpret_cast<ReturnType(*)(id, SEL, Args...)> (objc_msgSend) (target, getSelector (selector), args...);
    }

    static inline id getNSString (const char* s)           { return call<id> (getClass ("NSString"), "stringWithUTF8String:", s); }
    static inline id getNSString (const std::string& s)    { return getNSString (s.c_str()); }
    static inline id getNSNumberBool (bool b)              { return call<id> (getClass ("NSNumber"), "numberWithBool:", (BOOL) b); }
    static inline id getSharedNSApplication()              { return call<id> (getClass ("NSApplication"), "sharedApplication"); }
}

inline void choc::messageloop::run()
{
    objc::call<void> (objc::getSharedNSApplication(), "run");
}

inline void choc::messageloop::stop()
{
    using namespace choc::objc;
    static constexpr long NSEventTypeApplicationDefined = 15;

    call<void> (getSharedNSApplication(), "stop:", (id) nullptr);

    // After sending the stop message, we need to post a dummy event to
    // kick the message loop, otherwise it can just sit there and hang
    struct NSPoint { double x = 0, y = 0; };
    id dummyEvent = call<id> (getClass ("NSEvent"), "otherEventWithType:location:modifierFlags:timestamp:windowNumber:context:subtype:data1:data2:",
                              NSEventTypeApplicationDefined, NSPoint(), 0, 0, 0, nullptr, (short) 0, 0, 0);
    call<void> (getSharedNSApplication(), "postEvent:atStart:", dummyEvent, YES);
}

inline void choc::messageloop::postMessage (std::function<void()>&& fn)
{
    dispatch_async_f (dispatch_get_main_queue(),
                      new std::function<void()> (std::move (fn)),
                      (dispatch_function_t) (+[](void* arg)
                      {
                          std::unique_ptr<std::function<void()>> f (static_cast<std::function<void()>*> (arg));
                          (*f)();
                      }));
}

//==============================================================================
#elif CHOC_WINDOWS

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#include <windows.h>

namespace choc::messageloop
{

static DWORD& getMainThreadID()
{
    static DWORD threadID = GetCurrentThreadId();
    return threadID;
}

inline void run()
{
    getMainThreadID() = GetCurrentThreadId();

    for (;;)
    {
        MSG msg;

        if (GetMessage (std::addressof (msg), nullptr, 0, 0) == -1)
            break;

        if (msg.message == WM_APP)
        {
            std::unique_ptr<std::function<void()>> f (reinterpret_cast<std::function<void()>*> (msg.lParam));
            (*f)();
        }

        if (msg.message == WM_QUIT)
            break;

        if (msg.hwnd)
        {
            TranslateMessage (std::addressof (msg));
            DispatchMessage (std::addressof (msg));
        }
    }
}

inline void stop()
{
    PostQuitMessage (0);
}

inline void postMessage (std::function<void()>&& fn)
{
    PostThreadMessage (getMainThreadID(), WM_APP, 0, (LPARAM) new std::function<void()> (std::move (fn)));
}

} // namespace choc::messageloop

#else
 #error "choc::messageloop only supports OSX, Windows or Linux!"
#endif

#endif // CHOC_MESSAGELOOP_HEADER_INCLUDED
