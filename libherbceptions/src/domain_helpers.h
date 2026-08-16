#pragma once
/*
Shared helpers for the herbception error-domain runtime.

These are internal to the runtime implementation (used by the per-domain
translation units) and are not part of the public herbception/error surface.
*/
#include "herbceptions/error"

namespace std::error_domains {
namespace __herbceptions_detail {

// Write a single scatter into the IO-cookie collector. The cookie function
// only writes raw bytes; encoding conversion (codecvt) is done by the caller.
inline void write_text(::std::error_reporter_encoding encoding, void *cookie,
                       ::std::error_reporter_io_cookie_function cookfun,
                       void const *base, ::std::size_t len) noexcept {
  ::std::io_scatter_t v{base, len};
  cookfun(cookie, __builtin_addressof(v), 1u);
  (void)encoding;
}

// Largest stack buffer (in code units) used when codecvt-converting pieces to
// UTF-16/UTF-32.
inline constexpr ::std::size_t query_information_capacity = 512;

// Decode one UTF-8 code point at \p s, advancing \p i; returns the code point
// or (char32_t)-1 on invalid input.
constexpr char32_t utf8_decode(char8_t const *s, ::std::size_t len,
                               ::std::size_t &i) noexcept {
  char8_t const c = s[i];
  ::std::size_t extra = 0;
  char32_t cp = 0;
  if (c < 0x80) {
    cp = static_cast<char32_t>(c);
  } else if ((c & 0xE0) == 0xC0) {
    cp = c & 0x1F;
    extra = 1;
  } else if ((c & 0xF0) == 0xE0) {
    cp = c & 0x0F;
    extra = 2;
  } else if ((c & 0xF8) == 0xF0) {
    cp = c & 0x07;
    extra = 3;
  } else {
    return static_cast<char32_t>(-1);
  }
  if (i + extra >= len)
    return static_cast<char32_t>(-1);
  for (::std::size_t j = 0; j != extra; ++j) {
    char8_t const t = s[i + 1 + j];
    if ((t & 0xC0) != 0x80)
      return static_cast<char32_t>(-1);
    cp = (cp << 6) | static_cast<char32_t>(t & 0x3F);
  }
  i += extra + 1;
  return cp;
}

// Codecvt one UTF-8 piece to UTF-16 (with surrogate pairs for code points
// beyond the BMP). Returns the number of code units written, or (size_t)-1 if
// the input is invalid or the buffer is too small.
inline ::std::size_t utf8_to_utf16(char8_t const *s, ::std::size_t len,
                            char16_t *out,
                            ::std::size_t capacity) noexcept {
  ::std::size_t o = 0;
  for (::std::size_t i = 0; i != len;) {
    char32_t const cp = utf8_decode(s, len, i);
    if (cp == static_cast<char32_t>(-1))
      return static_cast<::std::size_t>(-1);
    if (cp >= 0x10000) {
      if (o + 2 > capacity)
        return static_cast<::std::size_t>(-1);
      char32_t const v = cp - 0x10000;
      out[o++] = static_cast<char16_t>(0xD800 + (v >> 10));
      out[o++] = static_cast<char16_t>(0xDC00 + (v & 0x3FF));
    } else {
      if (o + 1 > capacity)
        return static_cast<::std::size_t>(-1);
      out[o++] = static_cast<char16_t>(cp);
    }
  }
  return o;
}

// Codecvt one UTF-8 piece to UTF-32. Returns the number of code units written,
// or (size_t)-1 if the input is invalid or the buffer is too small.
inline ::std::size_t utf8_to_utf32(char8_t const *s, ::std::size_t len,
                            char32_t *out,
                            ::std::size_t capacity) noexcept {
  ::std::size_t o = 0;
  for (::std::size_t i = 0; i != len;) {
    char32_t const cp = utf8_decode(s, len, i);
    if (cp == static_cast<char32_t>(-1))
      return static_cast<::std::size_t>(-1);
    if (o + 1 > capacity)
      return static_cast<::std::size_t>(-1);
    out[o++] = cp;
  }
  return o;
}

// A writev-style collector of name/message pieces. The do_query_information
// handlers add the domain name and/or the per-code message as separate
// io_scatter_t entries (each pointing at its own storage — no copying); the
// whole array is then emitted in a single cookfun call. This mirrors POSIX
// writev: concatenation without an intermediate buffer. Encoding conversion
// and copying are the IO device's job (the encoding flag is passed through
// untouched), not ours.
//
// Maximum pieces: name (1) + message (up to a few, e.g. cxa_exception's
// "cxa_exception" + "(" + type + ")").
inline constexpr ::std::size_t query_information_max_pieces = 8;

struct query_information_pieces {
  ::std::io_scatter_t pieces[query_information_max_pieces];
  ::std::size_t count = 0;

  void add(char8_t const *s, ::std::size_t len) noexcept {
    if (count < query_information_max_pieces)
      pieces[count++] = {s, len};
  }
  void add_cstr(char8_t const *s) noexcept {
    add(s, __builtin_strlen(reinterpret_cast<char const *>(s)));
  }

  // Emit the accumulated pieces in the requested encoding. Codecvt is our job
  // (the device only writes bytes): byte-oriented encodings pass the UTF-8/u8
  // bytes through unchanged; utf16/utf32 are converted into a stack buffer and
  // the converted pieces are handed to the device as scatters.
  void emit(::std::error_reporter_encoding encoding, void *cookie,
            ::std::error_reporter_io_cookie_function cookfun) const noexcept {
    if (count == 0)
      return;
    switch (encoding) {
    case ::std::error_reporter_encoding::utf8:
    case ::std::error_reporter_encoding::gb18030:
    case ::std::error_reporter_encoding::utfebcdic:
      cookfun(cookie, pieces, count);
      return;
    case ::std::error_reporter_encoding::utf16: {
      char16_t buf[query_information_capacity];
      ::std::io_scatter_t converted[query_information_max_pieces];
      ::std::size_t off = 0;
      for (::std::size_t i = 0; i != count; ++i) {
        ::std::size_t const n = utf8_to_utf16(
            static_cast<char8_t const *>(pieces[i].base), pieces[i].len,
            buf + off, query_information_capacity - off);
        if (n == static_cast<::std::size_t>(-1))
          return;
        converted[i] = {buf + off, n * sizeof(char16_t)};
        off += n;
      }
      cookfun(cookie, converted, count);
      return;
    }
    case ::std::error_reporter_encoding::utf32: {
      char32_t buf[query_information_capacity];
      ::std::io_scatter_t converted[query_information_max_pieces];
      ::std::size_t off = 0;
      for (::std::size_t i = 0; i != count; ++i) {
        ::std::size_t const n = utf8_to_utf32(
            static_cast<char8_t const *>(pieces[i].base), pieces[i].len,
            buf + off, query_information_capacity - off);
        if (n == static_cast<::std::size_t>(-1))
          return;
        converted[i] = {buf + off, n * sizeof(char32_t)};
        off += n;
      }
      cookfun(cookie, converted, count);
      return;
    }
    }
  }
};

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
