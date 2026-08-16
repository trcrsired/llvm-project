#pragma once
/*
Shared helpers for the herbception error-domain runtime.

These are internal to the runtime implementation (used by the per-domain
translation units) and are not part of the public herbception/error surface.
*/
#include "herbceptions/error"

namespace std::error_domains {
namespace __herbceptions_detail {

// Write a single scatter into the IO-cookie collector.
inline void write_text(::std::error_reporter_encoding encoding, void *cookie,
                       ::std::error_reporter_io_cookie_function cookfun,
                       void const *base, ::std::size_t len) noexcept {
  ::std::io_scatter_t v{base, len};
  cookfun(encoding, cookie, __builtin_addressof(v), 1u);
}

// Write a fixed ASCII string in the requested encoding. The strings used for
// names/messages here are ASCII-only, so they are valid in every encoding.
// The string is a u8"" literal (char8_t); the bytes are passed through
// unchanged for every byte-oriented output encoding.
inline void write_ascii(::std::error_reporter_encoding encoding, void *cookie,
                        ::std::error_reporter_io_cookie_function cookfun,
                        char8_t const *s) noexcept {
  write_text(encoding, cookie, cookfun, s,
             __builtin_strlen(reinterpret_cast<char const *>(s)));
}

// ---------------------------------------------------------------------------
// Static, thread-safe errno message table. strerror is not used because it is
// not guaranteed thread-safe; the messages here match the POSIX strerror text
// for the standard errno values. All strings are u8"" literals so they are
// UTF-8 regardless of the execution character set (never EBCDIC).
// ---------------------------------------------------------------------------
struct errc_message_entry {
  int code;
  char8_t const *message;
};

inline constexpr errc_message_entry errc_messages[] = {
    {0, u8"Success"},
    {1, u8"Operation not permitted"},
    {2, u8"No such file or directory"},
    {3, u8"No such process"},
    {4, u8"Interrupted system call"},
    {5, u8"Input/output error"},
    {6, u8"No such device or address"},
    {7, u8"Argument list too long"},
    {8, u8"Exec format error"},
    {9, u8"Bad file descriptor"},
    {10, u8"No child processes"},
    {11, u8"Resource temporarily unavailable"},
    {12, u8"Cannot allocate memory"},
    {13, u8"Permission denied"},
    {14, u8"Bad address"},
    {16, u8"Device or resource busy"},
    {17, u8"File exists"},
    {18, u8"Invalid cross-device link"},
    {19, u8"No such device"},
    {20, u8"Not a directory"},
    {21, u8"Is a directory"},
    {22, u8"Invalid argument"},
    {23, u8"Too many open files in system"},
    {24, u8"Too many open files"},
    {25, u8"Inappropriate ioctl for device"},
    {26, u8"Text file busy"},
    {27, u8"File too large"},
    {28, u8"No space left on device"},
    {29, u8"Illegal seek"},
    {30, u8"Read-only file system"},
    {31, u8"Too many links"},
    {32, u8"Broken pipe"},
    {33, u8"Numerical argument out of domain"},
    {34, u8"Numerical result out of range"},
    {35, u8"Resource deadlock avoided"},
    {36, u8"File name too long"},
    {37, u8"No locks available"},
    {38, u8"Function not implemented"},
    {39, u8"Directory not empty"},
    {40, u8"Too many levels of symbolic links"},
    {42, u8"No message of desired type"},
    {43, u8"Identifier removed"},
    {61, u8"No data available"},
    {67, u8"Link has been severed"},
    {71, u8"Protocol error"},
    {74, u8"Bad message"},
    {75, u8"Value too large for defined data type"},
    {84, u8"Invalid or incomplete multibyte or wide character"},
    {88, u8"Socket operation on non-socket"},
    {89, u8"Destination address required"},
    {90, u8"Message too long"},
    {91, u8"Protocol wrong type for socket"},
    {92, u8"Protocol not available"},
    {93, u8"Protocol not supported"},
    {95, u8"Operation not supported"},
    {97, u8"Address family not supported by protocol"},
    {98, u8"Address already in use"},
    {99, u8"Cannot assign requested address"},
    {100, u8"Network is down"},
    {101, u8"Network is unreachable"},
    {102, u8"Network dropped connection on reset"},
    {103, u8"Software caused connection abort"},
    {104, u8"Connection reset by peer"},
    {105, u8"No buffer space available"},
    {107, u8"Transport endpoint is not connected"},
    {110, u8"Connection timed out"},
    {111, u8"Connection refused"},
    {113, u8"Host is unreachable"},
    {114, u8"Operation already in progress"},
    {115, u8"Operation now in progress"},
    {125, u8"Operation canceled"},
    {130, u8"Owner died"},
    {131, u8"State not recoverable"},
};

inline char8_t const *errc_message(int code) noexcept {
  for (errc_message_entry const &e : errc_messages)
    if (e.code == code)
      return e.message;
  return u8"Unknown error";
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
