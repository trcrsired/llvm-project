ERRORS = [
    ("0", "Success"),

    ("EPERM", "Not owner",
        "defined(EPERM) && (!defined(EACCES) || (EPERM != EACCES))"),

    ("ENOENT", "No such file or directory"),

    ("ESRCH", "No such process"),
    ("EINTR", "Interrupted system call"),
    ("EIO", "I/O error"),

    ("ENXIO", "No such device or address",
        "defined(ENXIO) && (!defined(ENODEV) || (ENXIO != ENODEV))"),

    ("E2BIG", "Arg list too long"),
    ("ENOEXEC", "Exec format error"),
    ("EALREADY", "Socket already connected"),
    ("EBADF", "Bad file number"),
    ("ECHILD", "No children"),
    ("EDESTADDRREQ", "Destination address required"),
    ("EAGAIN", "No more processes"),
    ("ENOMEM", "Not enough space"),
    ("EACCES", "Permission denied"),
    ("EFAULT", "Bad address"),
    ("ENOTBLK", "Block device required"),
    ("EBUSY", "Device or resource busy"),
    ("EEXIST", "File exists"),
    ("EXDEV", "Cross-device link"),
    ("ENODEV", "No such device"),
    ("ENOTDIR", "Not a directory"),
    ("EHOSTDOWN", "Host is down"),
    ("EINPROGRESS", "Connection already in progress"),
    ("EISDIR", "Is a directory"),
    ("EINVAL", "Invalid argument"),
    ("ENETDOWN", "Network interface is not configured"),
    ("ENETRESET", "Connection aborted by network"),
    ("ENFILE", "Too many open files in system"),
    ("EMFILE", "File descriptor value too large"),
    ("ENOTTY", "Not a character device"),
    ("ETXTBSY", "Text file busy"),
    ("EFBIG", "File too large"),
    ("EHOSTUNREACH", "Host is unreachable"),
    ("ENOSPC", "No space left on device"),
    ("ENOTSUP", "Not supported"),
    ("ESPIPE", "Illegal seek"),
    ("EROFS", "Read-only file system"),
    ("EMLINK", "Too many links"),
    ("EPIPE", "Broken pipe"),
    ("EDOM", "Mathematics argument out of domain of function"),
    ("ERANGE", "Result too large"),
    ("ENOMSG", "No message of desired type"),
    ("EIDRM", "Identifier removed"),
    ("EILSEQ", "Illegal byte sequence"),
    ("EDEADLK", "Deadlock"),
    ("ENETUNREACH", "Network is unreachable"),
    ("ENOLCK", "No lock"),
    ("ENOSTR", "Not a stream"),
    ("ETIME", "Stream ioctl timeout"),
    ("ENOSR", "No stream resources"),
    ("ENONET", "Machine is not on the network"),
    ("ENOPKG", "No package"),
    ("EREMOTE", "Resource is remote"),
    ("ENOLINK", "Virtual circuit is gone"),
    ("EADV", "Advertise error"),
    ("ESRMNT", "Srmount error"),
    ("ECOMM", "Communication error"),
    ("EPROTO", "Protocol error"),
    ("EPROTONOSUPPORT", "Unknown protocol"),
    ("EMULTIHOP", "Multihop attempted"),
    ("EBADMSG", "Bad message"),
    ("ELIBACC", "Cannot access a needed shared library"),
    ("ELIBBAD", "Accessing a corrupted shared library"),
    ("ELIBSCN", ".lib section in a.out corrupted"),
    ("ELIBMAX", "Attempting to link in more shared libraries than system limit"),
    ("ELIBEXEC", "Cannot exec a shared library directly"),
    ("ENOSYS", "Function not implemented"),
    ("ENMFILE", "No more files"),
    ("ENOTEMPTY", "Directory not empty"),
    ("ENAMETOOLONG", "File or path name too long"),
    ("ELOOP", "Too many symbolic links"),
    ("ENOBUFS", "No buffer space available"),
    ("ENODATA", "No data"),
    ("EAFNOSUPPORT", "Address family not supported by protocol family"),
    ("EPROTOTYPE", "Protocol wrong type for socket"),
    ("ENOTSOCK", "Socket operation on non-socket"),
    ("ENOPROTOOPT", "Protocol not available"),
    ("ESHUTDOWN", "Can't send after socket shutdown"),
    ("ECONNREFUSED", "Connection refused"),
    ("ECONNRESET", "Connection reset by peer"),
    ("EADDRINUSE", "Address already in use"),
    ("EADDRNOTAVAIL", "Address not available"),
    ("ECONNABORTED", "Software caused connection abort"),

    ("EWOULDBLOCK", "Operation would block",
        "(defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN)))"),

    ("ENOTCONN", "Socket is not connected"),
    ("ESOCKTNOSUPPORT", "Socket type not supported"),
    ("EISCONN", "Socket is already connected"),
    ("ECANCELED", "Operation canceled"),
    ("ENOTRECOVERABLE", "State not recoverable"),
    ("EOWNERDEAD", "Previous owner died"),
    ("ESTRPIPE", "Streams pipe error"),

    ("EOPNOTSUPP", "Operation not supported on socket",
        "defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))"),

    ("EOVERFLOW", "Value too large for defined data type"),
    ("EMSGSIZE", "Message too long"),
    ("ETIMEDOUT", "Connection timed out"),

    ("UNKNOWN", "Unknown")
]


# ---------------------------------------------------------------------------
# Generator
# ---------------------------------------------------------------------------
# Emits src/posix_table.hpp: a switch body fragment included inside
# __to_u8scatter_from_errno (posix.cpp). On top it defines
# POSIX_ERRNO_MAX_SIZE: the maximum message length in code units, used by
# posix.cpp to size the stack buffer for encoding conversion.
#
# Entry forms:
#   ("0", "msg")            -> unconditional `case 0:`
#   ("NAME", "msg")         -> `#ifdef NAME` ... `#endif`
#   ("NAME", "msg", "cond") -> `#if cond` ... `#endif`
#   ("UNKNOWN", "msg")      -> unconditional `default:`
#
# Conditions are emitted as-is. EWOULDBLOCK's condition is parenthesized in
# the generated file to match the original hand-written table.

import os
import re
import sys

OUTPUT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "src", "posix_table.hpp")

# Preserved comments, keyed by errno name, emitted just before the #if line.
COMMENTS = {
    "ENXIO": "/* go32 defines ENXIO as ENODEV */",
}


def max_message_size() -> int:
    # All messages are ASCII, so byte length == code-unit length.
    return max(len(msg) for _, msg, *_ in ERRORS)


def emit() -> str:
    lines = ["// clang-format off", f"#define POSIX_ERRC_MAX_SIZE {max_message_size()}", ""]
    for name, msg, *rest in ERRORS:
        if name == "0":
            lines.append("\tcase 0:")
        elif name == "UNKNOWN":
            lines.append("\tdefault:")
        else:
            if name in COMMENTS:
                lines.append(COMMENTS[name])
            if rest:
                lines.append(f"#if {rest[0]}")
            else:
                lines.append(f"#ifdef {name}")
            lines.append(f"\tcase {name}:")
        lines.append(f"\t\treturn __tsc(u8\"{msg}\");")
        if name not in ("0", "UNKNOWN"):
            lines.append("#endif")
    lines.append("// clang-format on")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Generic fragment writer, shared by the parse / cmath / wine tables.
#
# rows: iterable of (case_label, message) where case_label is an int (emitted
# as a bare numeric case, no macro guards) and None denotes the default row.
# Every fragment defines <max_macro> = the longest message length.
# ---------------------------------------------------------------------------
def write_fragment(path: str, max_macro: str, rows, default_msg: str) -> None:
    all_msgs = [msg for _, msg in rows] + [default_msg]
    lines = ["// clang-format off", f"#define {max_macro} {max(len(m) for m in all_msgs)}", ""]
    wrote_default = False
    for label, msg in rows:
        if label is None:
            lines.append("\tdefault:")
            wrote_default = True
        else:
            lines.append(f"\tcase {label}:")
        lines.append(f"\t\treturn __tsc(u8\"{msg}\");")
    if not wrote_default:
        lines.append("\tdefault:")
        lines.append(f"\t\treturn __tsc(u8\"{default_msg}\");")
    lines.append("// clang-format on")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {os.path.abspath(path)}")


# fast_io's parse_errc (include/herbceptions/__details/parse.h uses the same
# fixed numbering).
PARSE_ERRORS = [
    (0, "Success"),
    (1, "End of file"),
    (2, "Partial parse"),
    (3, "Invalid format"),
    (4, "Overflow"),
]

# cmath_errc is a bitmask over the C floating-point exception macros. Case
# labels are spelled through ::std::cmath_errc so they automatically track
# whatever values cmath_errc.h normalized for this toolchain (_FE_*, FE_*,
# or the built-in fallback); no macro guards needed. The consuming TU must
# include herbceptions/__details/cmath_errc.h before the fragment.
CMATH_ERRORS = [
    ("invalid", "Invalid floating point operation"),
    ("divbyzero", "Floating point divide by zero"),
    ("inexact", "Inexact floating point result"),
    ("overflow", "Floating point overflow"),
    ("underflow", "Floating point underflow"),
    ("all_except", "All floating point exceptions"),
]


def emit_cmath() -> str:
    default_msg = "Unknown"
    msgs = [msg for _, msg in CMATH_ERRORS] + [default_msg]
    lines = [
        "// clang-format off",
        "// Requires herbceptions/__details/cmath_errc.h (defines ::std::cmath_errc).",
        f"#define CMATH_ERRC_MAX_SIZE {max(len(m) for m in msgs)}",
        "",
    ]
    for name, msg in CMATH_ERRORS:
        lines.append(f"\tcase ::std::cmath_errc::{name}:")
        lines.append(f"\t\treturn __tsc(u8\"{msg}\");")
    lines.append("\tdefault:")
    lines.append(f"\t\treturn __tsc(u8\"{default_msg}\");")
    lines.append("// clang-format on")
    return "\n".join(lines) + "\n"


# The wine errno numbering is fixed (Linux kernel values vendored into
# fast_io), so raw numeric cases need no macro guards. Messages are parsed
# from the trailing /* ... */ comments of the vendored copy of
# __wine_unix_errno.h (kept in utils/ so the generator is self-contained;
# libherbceptions itself never references fast_io).
WINE_ERRNO_HEADER = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "__wine_unix_errno.h"
)

_WINE_ROW_RE = re.compile(
    r"^#define\s+__WINE_UNIX_ERRNO_([A-Z0-9_]+)\s+(\d+)\s*(?:/\*\s*(.+?)\s*\*/)?\s*$"
)


def wine_rows():
    rows = []
    with open(WINE_ERRNO_HEADER, "r") as f:
        for line in f:
            m = _WINE_ROW_RE.match(line)
            if not m:
                continue
            name, num, msg = m.group(1), int(m.group(2)), m.group(3)
            if msg is None:
                msg = name.replace("_", " ").capitalize()
            rows.append((num, msg))
    rows.sort(key=lambda r: r[0])
    return rows


def main() -> None:
    content = emit()
    with open(OUTPUT, "w", newline="\n") as f:
        f.write(content)
    print(f"wrote {os.path.abspath(OUTPUT)}")

    src = os.path.dirname(OUTPUT)
    write_fragment(os.path.join(src, "parse_table.hpp"), "PARSE_ERRC_MAX_SIZE",
                   PARSE_ERRORS, "Unknown")

    with open(os.path.join(src, "cmath_table.hpp"), "w", newline="\n") as f:
        f.write(emit_cmath())
    print(f"wrote {os.path.abspath(os.path.join(src, 'cmath_table.hpp'))}")

    wine = wine_rows()
    write_fragment(os.path.join(src, "wine_table.hpp"), "WINE_ERRC_MAX_SIZE",
                   wine, "Unknown")


if __name__ == "__main__":
    main()


