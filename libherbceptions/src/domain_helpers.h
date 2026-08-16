#pragma once
/*
Shared helpers for the herbception error-domain runtime.

These are internal to the runtime implementation (used by the per-domain
translation units) and are not part of the public herbception/error surface.
*/
#include "herbceptions/error"

namespace std::error_domains
{
namespace __herbceptions_detail
{

// Write a single scatter into the IO-cookie collector.
inline void write_text(::std::error_reporter_encoding encoding, void* cookie,
                       ::std::error_reporter_io_cookie_function cookfun,
                       void const* base, ::std::size_t len) noexcept
{
    ::std::io_scatter_t v{base, len};
    cookfun(encoding, cookie, __builtin_addressof(v), 1u);
}

// Write a fixed ASCII string in the requested encoding. The strings used for
// names/messages here are ASCII-only, so they are valid in every encoding.
inline void write_ascii(::std::error_reporter_encoding encoding, void* cookie,
                        ::std::error_reporter_io_cookie_function cookfun,
                        char const* s) noexcept
{
    write_text(encoding, cookie, cookfun, s, __builtin_strlen(s));
}

// ---------------------------------------------------------------------------
// Static, thread-safe errno message table. strerror is not used because it is
// not guaranteed thread-safe; the messages here match the POSIX strerror text
// for the standard errno values.
// ---------------------------------------------------------------------------
struct errc_message_entry
{
    int code;
    char const* message;
};

inline constexpr errc_message_entry errc_messages[] = {
    {0, "Success"},
    {1, "Operation not permitted"},
    {2, "No such file or directory"},
    {3, "No such process"},
    {4, "Interrupted system call"},
    {5, "Input/output error"},
    {6, "No such device or address"},
    {7, "Argument list too long"},
    {8, "Exec format error"},
    {9, "Bad file descriptor"},
    {10, "No child processes"},
    {11, "Resource temporarily unavailable"},
    {12, "Cannot allocate memory"},
    {13, "Permission denied"},
    {14, "Bad address"},
    {16, "Device or resource busy"},
    {17, "File exists"},
    {18, "Invalid cross-device link"},
    {19, "No such device"},
    {20, "Not a directory"},
    {21, "Is a directory"},
    {22, "Invalid argument"},
    {23, "Too many open files in system"},
    {24, "Too many open files"},
    {25, "Inappropriate ioctl for device"},
    {26, "Text file busy"},
    {27, "File too large"},
    {28, "No space left on device"},
    {29, "Illegal seek"},
    {30, "Read-only file system"},
    {31, "Too many links"},
    {32, "Broken pipe"},
    {33, "Numerical argument out of domain"},
    {34, "Numerical result out of range"},
    {35, "Resource deadlock avoided"},
    {36, "File name too long"},
    {37, "No locks available"},
    {38, "Function not implemented"},
    {39, "Directory not empty"},
    {40, "Too many levels of symbolic links"},
    {42, "No message of desired type"},
    {43, "Identifier removed"},
    {61, "No data available"},
    {67, "Link has been severed"},
    {71, "Protocol error"},
    {74, "Bad message"},
    {75, "Value too large for defined data type"},
    {84, "Invalid or incomplete multibyte or wide character"},
    {88, "Socket operation on non-socket"},
    {89, "Destination address required"},
    {90, "Message too long"},
    {91, "Protocol wrong type for socket"},
    {92, "Protocol not available"},
    {93, "Protocol not supported"},
    {95, "Operation not supported"},
    {97, "Address family not supported by protocol"},
    {98, "Address already in use"},
    {99, "Cannot assign requested address"},
    {100, "Network is down"},
    {101, "Network is unreachable"},
    {102, "Network dropped connection on reset"},
    {103, "Software caused connection abort"},
    {104, "Connection reset by peer"},
    {105, "No buffer space available"},
    {107, "Transport endpoint is not connected"},
    {110, "Connection timed out"},
    {111, "Connection refused"},
    {113, "Host is unreachable"},
    {114, "Operation already in progress"},
    {115, "Operation now in progress"},
    {125, "Operation canceled"},
    {130, "Owner died"},
    {131, "State not recoverable"},
};

inline char const* errc_message(int code) noexcept
{
    for (const errc_message_entry& e : errc_messages)
        if (e.code == code)
            return e.message;
    return "Unknown error";
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
