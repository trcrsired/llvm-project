# Generates the win32 -> posix errno and nt (NTSTATUS) -> posix errno switch
# fragments consumed by src/win32.cpp and src/nt.cpp:
#
#   src/win32_errc_map.hpp   included inside the win32 do_to_errc switch
#   src/nt_errc_map.hpp      included inside the nt do_to_errc switch
#
# Data sources (first source wins on conflicts; in practice they agree):
#
#   1. ntkernel-error-category's ntkernel-table.ipp (Apache-2.0 / Boost-1.0,
#      Niall Douglas). Its rows are NTSTATUS -> {win32, posix errno}: the nt
#      table reads its posix column directly, and the win32 table is the
#      inversion of its {win32, posix} columns.
#
#   2. status-code's generated mapping tables (Apache-2.0 / Boost-1.0, Niall
#      Douglas): win32_code_to_generic_code.ipp and
#      nt_code_to_generic_code.ipp, which fill in codes the ntkernel table
#      leaves unmapped.
#
# Every row is guarded on #ifdef <ERRNO> so it vanishes on toolchains that do
# not define that errno. Errnos with a dedicated std::errc enumerator return
# the enumerator; the rest pass their value through static_cast like
# wine_errc_map.hpp does.

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SCRIPT_DIR, "..", "src")

DEFAULT_NTKERNEL_DIR = os.path.join(
    os.path.expanduser("~"), "toolchains_build", "ntkernel-error-category")
DEFAULT_STATUS_CODE_DIR = os.path.join(
    os.path.expanduser("~"), "toolchains_build", "status-code")

# errno macro -> std::errc enumerator. Errnos absent from this map are emitted
# as static_cast<::std::errc>(<errno>) passthroughs.
ERRNO_TO_ERRC = {
    "EPERM": "operation_not_permitted",
    "ENOENT": "no_such_file_or_directory",
    "ESRCH": "no_such_process",
    "EINTR": "interrupted",
    "EIO": "io_error",
    "ENXIO": "no_such_device_or_address",
    "E2BIG": "argument_list_too_long",
    "ENOEXEC": "executable_format_error",
    "EBADF": "bad_file_descriptor",
    "ECHILD": "no_child_process",
    "EAGAIN": "resource_unavailable_try_again",
    "ENOMEM": "not_enough_memory",
    "EACCES": "permission_denied",
    "EFAULT": "bad_address",
    "EBUSY": "device_or_resource_busy",
    "EEXIST": "file_exists",
    "EXDEV": "cross_device_link",
    "ENODEV": "no_such_device",
    "ENOTDIR": "not_a_directory",
    "EISDIR": "is_a_directory",
    "EINVAL": "invalid_argument",
    "ENFILE": "too_many_files_open_in_system",
    "EMFILE": "too_many_files_open",
    "ENOTTY": "inappropriate_io_control_operation",
    "ETXTBSY": "text_file_busy",
    "EFBIG": "file_too_large",
    "ENOSPC": "no_space_on_device",
    "ESPIPE": "invalid_seek",
    "EROFS": "read_only_file_system",
    "EMLINK": "too_many_links",
    "EPIPE": "broken_pipe",
    "EDOM": "argument_out_of_domain",
    "ERANGE": "result_out_of_range",
    "EDEADLK": "resource_deadlock_would_occur",
    "ENAMETOOLONG": "filename_too_long",
    "ENOLCK": "no_lock_available",
    "ENOSYS": "function_not_supported",
    "ENOTEMPTY": "directory_not_empty",
    "ELOOP": "too_many_symbolic_link_levels",
    "EOVERFLOW": "value_too_large",
    "EWOULDBLOCK": "operation_would_block",
    "EMSGSIZE": "message_size",
    "EPROTOTYPE": "wrong_protocol_type",
    "ENOPROTOOPT": "no_protocol_option",
    "EPROTONOSUPPORT": "protocol_not_supported",
    "EOPNOTSUPP": "operation_not_supported",
    "ENOTSUP": "operation_not_supported",
    "EAFNOSUPPORT": "address_family_not_supported",
    "EADDRINUSE": "address_in_use",
    "EADDRNOTAVAIL": "address_not_available",
    "ENETDOWN": "network_down",
    "ENETUNREACH": "network_unreachable",
    "ENETRESET": "network_reset",
    "ECONNABORTED": "connection_aborted",
    "ECONNRESET": "connection_reset",
    "EISCONN": "already_connected",
    "ENOTCONN": "not_connected",
    "ETIMEDOUT": "timed_out",
    "ECONNREFUSED": "connection_refused",
    "EHOSTUNREACH": "host_unreachable",
    "EALREADY": "connection_already_in_progress",
    "EINPROGRESS": "operation_in_progress",
    "EDESTADDRREQ": "destination_address_required",
    "ENOBUFS": "no_buffer_space",
    "ENOTSOCK": "not_a_socket",
    "EDQUOT": None,
    "ESTALE": None,
    "ESHUTDOWN": None,
    "EHOSTDOWN": None,
    "EREMOTE": None,
    "EUSERS": None,
    "ECOMM": "protocol_error",
    "EPROTO": "protocol_error",
    "EBADMSG": "bad_message",
    "EOVERFLOW": "value_too_large",
    "EILSEQ": "illegal_byte_sequence",
    "ECANCELED": "operation_canceled",
    "ENOTRECOVERABLE": "state_not_recoverable",
    "EOWNERDEAD": "owner_dead",
    "ENOMSG": "no_message",
    "EIDRM": "identifier_removed",
    "ENODATA": "no_message_available",
    "ENOSR": "no_stream_resources",
    "ETIME": "stream_timeout",
}

_NTKERNEL_ROW_RE = re.compile(
    r"\{\s*static_cast<[^>]+>\((0x[0-9a-fA-F]+)\)\s*,"
    r"\s*static_cast<[^>]+>\((0x[0-9a-fA-F]+)\)\s*,"
    r"\s*([A-Z][A-Z0-9_]*|0)\s*,")

_SC_ROW_RE = re.compile(
    r"^case\s+(0x[0-9a-fA-F]+)\s*:\s*return\s+([A-Z][A-Z0-9_]*)\s*;")


def parse_ntkernel_table(path):
    """Yields (ntstatus, win32, errno_name_or_None) per ntkernel-table row."""
    rows = []
    with open(path, "r") as f:
        for line in f:
            m = _NTKERNEL_ROW_RE.search(line)
            if not m:
                continue
            nt = int(m.group(1), 16)
            win32 = int(m.group(2), 16)
            errno = m.group(3) if m.group(3) != "0" else None
            rows.append((nt, win32, errno))
    return rows


def parse_status_code_table(path):
    """Yields (code, errno_name) per status-code generic-mapping row."""
    rows = []
    with open(path, "r") as f:
        for line in f:
            m = _SC_ROW_RE.match(line.strip())
            if m:
                rows.append((int(m.group(1), 16), m.group(2)))
    return rows


def merge(first, second):
    """Merges code -> errno dicts; entries from `first` win on conflict."""
    out = dict(second)
    out.update(first)
    return out


def emit_fragment(path, header_note, cases, success_case):
    lines = [
        "// clang-format off",
        "// Generated by utils/generate_win32_nt_tables.py; do not edit.",
        "// " + header_note,
        "// Requires <cerrno> and ::std::errc.",
        "",
    ]
    if success_case:
        lines += ["\tcase 0u:", "\t\treturn ::std::errc{};", ""]
    for code, errno in sorted(cases.items()):
        errc = ERRNO_TO_ERRC.get(errno, "__passthrough__")
        lines.append(f"#ifdef {errno}")
        lines.append(f"\tcase {hex(code)}u:")
        if errc == "__passthrough__":
            lines.append(f"\t\treturn static_cast<::std::errc>({errno});")
        else:
            lines.append(f"\t\treturn ::std::errc::{errc};")
        lines.append("#endif")
    lines += ["", "\tdefault:", "\t\treturn ::std::errc::io_error;",
              "// clang-format on"]
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {os.path.abspath(path)} ({len(cases)} mapped codes)")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ntkernel-dir", default=os.environ.get(
        "NTKERNEL_ERROR_CATEGORY_DIR", DEFAULT_NTKERNEL_DIR))
    ap.add_argument("--status-code-dir", default=os.environ.get(
        "STATUS_CODE_DIR", DEFAULT_STATUS_CODE_DIR))
    args = ap.parse_args()

    ntkernel_table = os.path.join(
        args.ntkernel_dir, "include", "ntkernel-error-category", "detail",
        "ntkernel-table.ipp")
    sc_win32_table = os.path.join(
        args.status_code_dir, "include", "status-code", "detail",
        "win32_code_to_generic_code.ipp")
    sc_nt_table = os.path.join(
        args.status_code_dir, "include", "status-code", "detail",
        "nt_code_to_generic_code.ipp")

    rows = parse_ntkernel_table(ntkernel_table)

    # nt -> errno: the ntkernel posix column, filled out by status-code.
    nt_from_ntkernel = {nt: e for nt, _, e in rows if e is not None}
    nt_cases = merge(nt_from_ntkernel,
                     dict(parse_status_code_table(sc_nt_table)))

    # win32 -> errno: invert the ntkernel {win32, posix} columns, then let
    # status-code's direct win32 table fill the gaps.
    win32_from_ntkernel = {}
    for _, win32, errno in rows:
        if win32 != 0 and errno is not None:
            win32_from_ntkernel.setdefault(win32, errno)
    win32_cases = merge(win32_from_ntkernel,
                        dict(parse_status_code_table(sc_win32_table)))

    unknown = {e for e in list(nt_cases.values()) + list(win32_cases.values())
               if e not in ERRNO_TO_ERRC}
    if unknown:
        print(f"warning: errnos without a known std::errc enumerator "
              f"(emitted as static_cast passthroughs): {sorted(unknown)}",
              file=sys.stderr)

    emit_fragment(os.path.join(SRC_DIR, "win32_errc_map.hpp"),
                  "win32 GetLastError() error code -> std::errc; case 0 is "
                  "ERROR_SUCCESS.", win32_cases, success_case=True)
    emit_fragment(os.path.join(SRC_DIR, "nt_errc_map.hpp"),
                  "failed NTSTATUS (severity bits set) -> std::errc; success "
                  "codes are handled by the caller.", nt_cases,
                  success_case=False)


if __name__ == "__main__":
    main()
