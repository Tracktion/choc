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

#ifndef CHOC_EXECUTE_HEADER_INCLUDED
#define CHOC_EXECUTE_HEADER_INCLUDED

#include <string>
#include <sstream>
#include <stdio.h>

#if CHOC_WINDOWS
 #undef  WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #undef  NOMINMAX
 #define NOMINMAX
 #define Rectangle Rectangle_renamed_to_avoid_name_collisions
 #include <windows.h>
 #undef Rectangle
#endif

namespace choc
{

/// The result of a call to the execute() function.
struct ProcessResult
{
    /// The process's output
    std::string output;

    /// The process's status code on exit. This is a standard unix-style
    /// exit code as returned, where 0 is success, and other values
    /// indicate failures.
    int statusCode = 0;
};

//==============================================================================
/// Executes a system command synchronously, returning when the spawned process
/// has terminated, and providing the command's output as a string, along with
/// the process's status code.
///
/// @param command The command to attempt to run.
/// @param alsoReadStdErr If set to true, the stderr stream will also be read and
/// returned as part of the output. If false, just stdout will be read.
///
/// This may throw an exception if something fails.
///
ProcessResult execute (std::string command,
                       bool alsoReadStdErr = false);





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


#if CHOC_WINDOWS

// On windows avoid _popen as it causes all handles to be visible to the new
// process, which can trip up logic in the caller (e.g. sharing violations)
inline ProcessResult execute (std::string command, bool alsoReadStdErr)
{
    struct ScopedHandle
    {
        ScopedHandle() = default;
        ScopedHandle (const ScopedHandle&) = delete;
        ScopedHandle& operator= (const ScopedHandle&) = delete;
        ~ScopedHandle()         { reset(); }

        bool isValid() const    { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }

        void reset()
        {
            if (isValid())
                ::CloseHandle (handle);

            handle = nullptr;
        }

        HANDLE handle = nullptr;
    };

    struct AttributeListDeleter
    {
        ~AttributeListDeleter()     { if (list != nullptr) ::DeleteProcThreadAttributeList (list); }

        LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    };

    auto throwLastError = [] (const char* operation)
    {
        throw std::runtime_error ("choc::execute() failed: " + std::string (operation) + " (error " + std::to_string (::GetLastError()) + ")");
    };

    auto makeInheritableCopy = [] (HANDLE source, ScopedHandle& dest)
    {
        if (! ::DuplicateHandle (::GetCurrentProcess(), source, ::GetCurrentProcess(), &dest.handle, 0, TRUE, DUPLICATE_SAME_ACCESS))
            dest.handle = nullptr;
    };

    SECURITY_ATTRIBUTES inheritableAttributes { sizeof (SECURITY_ATTRIBUTES), nullptr, TRUE };

    auto openNullDevice = [&] (DWORD access, ScopedHandle& dest)
    {
        dest.handle = ::CreateFileA ("NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritableAttributes, OPEN_EXISTING, 0, nullptr);
    };

    ScopedHandle readPipe, writePipe;

    if (! ::CreatePipe (&readPipe.handle, &writePipe.handle, nullptr, 0))
        throwLastError ("CreatePipe");

    ScopedHandle childInput, childOutput, childError;
    makeInheritableCopy (writePipe.handle, childOutput);
    writePipe.reset();

    if (! childOutput.isValid())
        throwLastError ("DuplicateHandle");

    // The child gets the null device as its stdin, so it can never block waiting for input
    openNullDevice (GENERIC_READ, childInput);

    if (! childInput.isValid())
        throwLastError ("CreateFile");

    // If stderr isn't being captured, it goes to this process's stderr (or the null device)
    if (! alsoReadStdErr)
    {
        auto parentError = ::GetStdHandle (STD_ERROR_HANDLE);

        if (parentError != nullptr && parentError != INVALID_HANDLE_VALUE)
            makeInheritableCopy (parentError, childError);

        if (! childError.isValid())
            openNullDevice (GENERIC_WRITE, childError);

        if (! childError.isValid())
            throwLastError ("CreateFile");
    }

    auto errorHandle = alsoReadStdErr ? childOutput.handle : childError.handle;

    std::vector<HANDLE> handlesToInherit { childInput.handle, childOutput.handle };

    if (errorHandle != childOutput.handle)
        handlesToInherit.push_back (errorHandle);

    SIZE_T attributeListSize = 0;
    ::InitializeProcThreadAttributeList (nullptr, 1, 0, &attributeListSize);
    std::vector<char> attributeListStorage (attributeListSize);
    auto attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST> (attributeListStorage.data());

    if (! ::InitializeProcThreadAttributeList (attributeList, 1, 0, &attributeListSize))
        throwLastError ("InitializeProcThreadAttributeList");

    AttributeListDeleter attributeListDeleter;
    attributeListDeleter.list = attributeList;

    if (! ::UpdateProcThreadAttribute (attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handlesToInherit.data(), handlesToInherit.size() * sizeof (HANDLE), nullptr, nullptr))
        throwLastError ("UpdateProcThreadAttribute");

    STARTUPINFOEXA startupInfo {};
    startupInfo.StartupInfo.cb = sizeof (startupInfo);
    startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.StartupInfo.hStdInput  = childInput.handle;
    startupInfo.StartupInfo.hStdOutput = childOutput.handle;
    startupInfo.StartupInfo.hStdError  = errorHandle;
    startupInfo.lpAttributeList = attributeList;

    std::string interpreter = "cmd.exe";

    if (auto length = ::GetEnvironmentVariableA ("COMSPEC", nullptr, 0); length > 0)
    {
        std::string comspec (length, '\0');
        comspec.resize (::GetEnvironmentVariableA ("COMSPEC", comspec.data(), length));

        if (! comspec.empty())
            interpreter = comspec;
    }

    auto commandLine = "\"" + interpreter + "\" /c " + command;

    PROCESS_INFORMATION processInfo {};

    if (! ::CreateProcessA (nullptr, commandLine.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startupInfo.StartupInfo, &processInfo))
        throwLastError ("CreateProcess");

    ScopedHandle process, thread;
    process.handle = processInfo.hProcess;
    thread.handle = processInfo.hThread;

    // Close our copies of the child's handles, so that the pipe reports end-of-file once the child (and anything it launches) has exited
    childInput.reset();
    childOutput.reset();
    childError.reset();

    std::string output;

    for (;;)
    {
        char buffer[4096];
        DWORD bytesRead = 0;

        if (! ::ReadFile (readPipe.handle, buffer, sizeof (buffer), &bytesRead, nullptr) || bytesRead == 0)
            break;

        output.append (buffer, bytesRead);
    }

    ::WaitForSingleObject (process.handle, INFINITE);

    DWORD exitCode = 0;

    if (! ::GetExitCodeProcess (process.handle, &exitCode))
        throwLastError ("GetExitCodeProcess");

    return { std::move (output), static_cast<int> (exitCode) };
}

#else

inline std::string getErrnoString()
{
    char errorText[256] = {};

    struct Helper
    {
        // overloads to handle two variants of strerror_r that return different things
        static const char* handleResult (int, const char* s)         { return s; }
        static const char* handleResult (const char* s, const char*) { return s; }
    };

    return Helper::handleResult (strerror_r (errno, errorText, sizeof(errorText)), errorText);
}

inline ProcessResult execute (std::string command, bool alsoReadStdErr)
{
    if (alsoReadStdErr)
        command += " 2>&1";

    auto handle = ::popen (command.c_str(), "r");

    if (! handle)
        throw std::runtime_error ("choc::execute() failed: " + getErrnoString());

    std::ostringstream result;

    for (;;)
    {
        char buffer[1024];
        auto bytesRead = ::fread (buffer, 1, sizeof (buffer), handle);

        try
        {
            result.write (buffer, (std::streamsize) bytesRead);

            if (bytesRead < sizeof (buffer))
                break;
        }
        catch (...) { break; }
    }

    auto status = ::pclose (handle);
    return { result.str(), status };
}

#endif

} // namespace choc

#endif  // CHOC_EXECUTE_HEADER_INCLUDED
