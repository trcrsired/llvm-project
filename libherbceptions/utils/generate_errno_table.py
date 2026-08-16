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
    lines = [f"#define POSIX_ERRNO_MAX_SIZE {max_message_size()}", ""]
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
    return "\n".join(lines) + "\n"


def main() -> None:
    content = emit()
    with open(OUTPUT, "w", newline="\n") as f:
        f.write(content)
    print(f"wrote {os.path.abspath(OUTPUT)}")


if __name__ == "__main__":
    main()


