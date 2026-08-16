#pragma once
/*
nt (nt_errc) error domain header.

Declares nt_errc (NTSTATUS STATUS_* codes) and its error_domain
specialization. The singleton vtable is implemented in src/nt.cpp. Only
available on _WIN32/__CYGWIN__ targets.
*/
#include "../error"
#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std
{

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

}

#endif // _WIN32 || __CYGWIN__
