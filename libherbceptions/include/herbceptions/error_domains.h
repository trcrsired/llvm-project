#pragma once
/*
Error domains runtime library headers.

Each error domain maps a set of error codes to the standard error model:
  - posix:  std::errc (POSIX errno values)
  - cxa_exception_code: legacy C++ exception carrier (see cxa_exception_code.h)
  - win32:  win32_errc (Win32 GetLastError() codes, ERROR_*)
  - nt:     nt_errc (NTSTATUS values, STATUS_*)
  - com:    com_errc (HRESULT values)
  - wine:   wine_errc (host POSIX errno when running under Wine)

On non-Windows targets only the posix and cxa_exception_code domains are
available; the win32/nt/com/wine domains are defined when _WIN32 or __CYGWIN__
is defined.

Every domain implements error_domain_singleton's vtable (do_equivalent,
do_name, do_message, do_to_errc, do_cleanup, do_throw_cxa_exception) so that
std::error values from different domains interoperate: do_equivalent converts
both sides to std::errc, and do_message/do_name produce human-readable text.
*/
#include "error"
#include "cxa_exception_code.h"
#include <cstdint>

namespace std
{

#if defined(_WIN32) || defined(__CYGWIN__)

// ---------------------------------------------------------------------------
// win32_errc — Win32 error codes (ERROR_*), GetLastError() values.
// ---------------------------------------------------------------------------
enum class win32_errc : ::std::uint_least32_t
{
    success=0,
    invalid_function=1,
    file_not_found=2,
    path_not_found=3,
    too_many_open_files=4,
    access_denied=5,
    invalid_handle=6,
    arena_trashed=7,
    not_enough_memory=8,
    invalid_block=9,
    bad_environment=10,
    bad_format=11,
    invalid_access=12,
    invalid_data=13,
    out_of_memory=14,
    no_more_files=18,
    write_protect=19,
    no_more_segments=20,
    wrong_disk=21,
    sector_not_found=27,
    out_of_paper=28,
    sharing_violation=32,
    lock_violation=33,
    handle_disk_full=39,
    insufficient_buffer=122,
    invalid_name=123,
    invalid_level=124,
    not_supported=50,
    remote_not_listening=51,
    duplicate_name=52,
    network_name_deleted=53,
    network_busy=54,
    device_not_available=55,
    too_many_commands=56,
    bad_remote_adapter=57,
    file_exists=80,
    cannot_make=82,
    fail_i24=83,
    out_of_structures=84,
    already_assigned=85,
    invalid_password=86,
    invalid_parameter=87,
    net_write_fault=88,
    no_proc_slots=89,
    too_many_semaphores=100,
    exclusive_semaphore_already_owned=101,
    semaphore_is_set=102,
    too_many_semaphore_requests=103,
    invalid_at_interrupt_time=104,
    semaphore_owner_dead=105,
    semaphore_user_limit=106,
    insert_disk=107,
    drive_locked=108,
    broken_pipe=109,
    open_failed=110,
    buffer_overflow=111,
    disk_full=112,
    no_more_search_handles=113,
    invalid_target_handle=114,
    protection_violation=115,
    handle_was_closed=116,
    loading_library=117,
    canceled=1223,
    operation_aborted=995,
    io_incomplete=996,
    io_pending=997,
    already_exists=183
};

template<>
class error_domain<::std::win32_errc>
{
public:
    using errc_type = ::std::win32_errc;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_win32();
    }
    static inline constexpr ::std::size_t code(errc_type __e) noexcept
    {
        return static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(__e));
    }
};

// ---------------------------------------------------------------------------
// nt_errc — NTSTATUS values (STATUS_*).
// ---------------------------------------------------------------------------
enum class nt_errc : ::std::uint_least32_t
{
    success=0x00000000,
    buffer_too_small=0xC0000023,
    access_denied=0xC0000022,
    invalid_handle=0xC0000008,
    invalid_parameter=0xC000000D,
    no_such_file=0xC000000F,
    end_of_file=0xC0000011,
    invalid_device_request=0xC0000010,
    more_processing_required=0xC0000016,
    insufficient_resources=0xC000009A,
    disk_full=0xC000007F,
    object_name_not_found=0xC0000034,
    object_path_not_found=0xC000003A,
    no_such_device=0xC000000E,
    device_not_ready=0xC00000B5,
    invalid_parameter_1=0xC00000EF,
    invalid_parameter_2=0xC00000F0,
    not_implemented=0xC0000002,
    internal_error=0xC00000E5,
    not_a_directory=0xC0000103,
    directory_not_empty=0xC0000101,
    object_name_invalid=0xC0000033,
    file_is_a_directory=0xC00000BA,
    privilege_not_held=0xC0000061,
    logon_failure=0xC000006D,
    account_restriction=0xC000006E,
    invalid_logon_hours=0xC000006F,
    invalid_workstation=0xC0000070,
    password_expired=0xC0000071,
    account_disabled=0xC0000072,
    too_many_links=0xC0000096,
    share_violation=0xC0000043,
    delete_pending=0xC0000056,
    file_locked=0xC000005E,
    too_many_opens=0xC000011A
};

template<>
class error_domain<::std::nt_errc>
{
public:
    using errc_type = ::std::nt_errc;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_nt();
    }
    static inline constexpr ::std::size_t code(errc_type __e) noexcept
    {
        return static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(__e));
    }
};

// ---------------------------------------------------------------------------
// com_errc — HRESULT values. The underlying type is uint32 so the standard
// 0x8000xxxx "failure" bit pattern fits without narrowing.
// ---------------------------------------------------------------------------
enum class com_errc : ::std::uint_least32_t
{
    ok=0x00000000,
    nointerface=0x80004002,
    fail=0x80004005,
    unexpected=0x8000FFFF,
    notimpl=0x80004001,
    outofmemory=0x8007000E,
    invalidarg=0x80070057,
    accessdenied=0x80070005,
    handle=0x80070006,
    abort=0x80004004,
    fail_ie=0x8000FFFD,
    pending=0x8000000A,
    cancelled=0x800704C7,
    notfound=0x80070490,
    alreadyexists=0x800700B7,
    nointerface_ie=0x8000FFFE
};

template<>
class error_domain<::std::com_errc>
{
public:
    using errc_type = ::std::com_errc;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_com();
    }
    static inline constexpr ::std::size_t code(errc_type __e) noexcept
    {
        return static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(__e));
    }
};

// ---------------------------------------------------------------------------
// wine_errc — Wine's own UNIX errno values, matching fast_io's
// src/__wine_unix/include/__wine_unix/__wine_unix_errno.h (Linux-kernel style
// numbering, referenced from the Linux kernel header). These are the values
// Wine reports for host-side errors; they differ from the Windows API error
// codes and are independent of the host libc's errno.
// ---------------------------------------------------------------------------
enum class wine_errc : ::std::int_least32_t
{
    success=0,                 // __WINE_UNIX_ERRNO_SUCCESS
    permission_denied=1,       // EPERM
    no_such_file=2,            // ENOENT
    no_such_process=3,         // ESRCH
    interrupted=4,             // EINTR
    io_error=5,                // EIO
    no_such_device_or_address=6,  // ENXIO
    argument_list_too_long=7,  // E2BIG
    exec_format_error=8,       // ENOEXEC
    bad_file_descriptor=9,     // EBADF
    no_child_process=10,       // ECHILD
    again=11,                  // EAGAIN
    no_memory=12,              // ENOMEM
    bad_address=14,            // EFAULT
    device_or_resource_busy=16,// EBUSY
    file_exists=17,            // EEXIST
    cross_device_link=18,      // EXDEV
    no_such_device=19,         // ENODEV
    not_a_directory=20,        // ENOTDIR
    is_a_directory=21,         // EISDIR
    invalid_argument=22,       // EINVAL
    too_many_files_open_in_system=23, // ENFILE
    too_many_files_open=24,    // EMFILE
    inappropriate_io_control_operation=25, // ENOTTY
    text_file_busy=26,         // ETXTBSY
    file_too_large=27,         // EFBIG
    no_space_on_device=28,     // ENOSPC
    invalid_seek=29,           // ESPIPE
    read_only_file_system=30,  // EROFS
    too_many_links=31,         // EMLINK
    broken_pipe=32,            // EPIPE
    argument_out_of_domain=33, // EDOM
    result_out_of_range=34,    // ERANGE
    resource_deadlock_would_occur=35, // EDEADLK
    filename_too_long=36,      // ENAMETOOLONG
    no_lock_available=37,      // ENOLCK
    function_not_supported=38, // ENOSYS
    directory_not_empty=39,    // ENOTEMPTY
    too_many_symbolic_link_levels=40, // ELOOP
    would_block=41,            // EWOULDBLOCK
    no_message=42,             // ENOMSG
    identifier_removed=43,     // EIDRM
    no_link=67,                // ENOLINK
    protocol_error=71,         // EPROTO
    bad_message=74,            // EBADMSG
    value_too_large=75,        // EOVERFLOW
    illegal_byte_sequence=84,  // EILSEQ
    not_a_socket=88,           // ENOTSOCK
    destination_address_required=89, // EDESTADDRREQ
    message_size=90,           // EMSGSIZE
    wrong_protocol_type=91,    // EPROTOTYPE
    no_protocol_option=92,     // ENOPROTOOPT
    protocol_not_supported=93, // EPROTONOSUPPORT
    not_supported=95,          // EOPNOTSUPP
    address_family_not_supported=97, // EAFNOSUPPORT
    address_in_use=98,         // EADDRINUSE
    address_not_available=99,  // EADDRNOTAVAIL
    network_down=100,          // ENETDOWN
    network_unreachable=101,   // ENETUNREACH
    connection_aborted=103,    // ECONNABORTED
    connection_reset=104,      // ECONNRESET
    no_buffer_space=105,       // ENOBUFS
    already_connected=106,     // EISCONN
    not_connected=107,         // ENOTCONN
    timed_out=110,             // ETIMEDOUT
    connection_refused=111,    // ECONNREFUSED
    host_unreachable=113,      // EHOSTUNREACH
    operation_in_progress=115, // EINPROGRESS
    stale_file_handle=116,     // ESTALE
    canceled=125,              // ECANCELED
    owner_dead=130,            // EOWNERDEAD
    state_not_recoverable=131, // ENOTRECOVERABLE
    operation_not_permitted=1, // EPERM
};

template<>
class error_domain<::std::wine_errc>
{
public:
    using errc_type = ::std::wine_errc;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_wine();
    }
    static inline constexpr ::std::size_t code(errc_type __e) noexcept
    {
        return static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(__e));
    }
};

#endif // _WIN32 || __CYGWIN__

// ---------------------------------------------------------------------------
// posix — std::errc. Available on all platforms.
// ---------------------------------------------------------------------------
template<>
class error_domain<::std::errc>
{
public:
    using errc_type = ::std::errc;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_posix();
    }
    static inline constexpr ::std::size_t code(errc_type __e) noexcept
    {
        return static_cast<::std::size_t>(
            static_cast<::std::underlying_type_t<errc_type>>(__e));
    }
};

}
