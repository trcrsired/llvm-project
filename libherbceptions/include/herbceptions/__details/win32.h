#pragma once
/*
win32 (win32_errc) error domain header.

Declares win32_errc (Win32 GetLastError codes, ERROR_*) and its error_domain
specialization. The singleton vtable is implemented in src/win32.cpp. Only
available on _WIN32/__CYGWIN__ targets.
*/
#include "../error"
#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std
{

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

}

#endif // _WIN32 || __CYGWIN__
