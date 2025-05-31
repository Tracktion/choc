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



// This empty-ish file exists so that we test the inclusion of all the
// headers in more than one compile unit, That should catch any
// non-inlined symbols that will cause duplicate function errors
// at link time.


// On Windows, we'll include windows.h before the headers, in contrast to what
// happens in main.cpp
#if defined (_WIN32) || defined (_WIN64)
 #include <windows.h>
#endif

#ifndef __MINGW32__
 // This can only be included in one translation unit
 #include "../audio/io/choc_RtAudioPlayer.h"
 #include "../audio/io/choc_RenderingAudioMIDIPlayer.h"
#endif

// This one pulls in windows.h so keep it out of choc_tests.h
#include "../platform/choc_MemoryDLL.h"

#include "choc_tests.h"
