// clang-format off
// Generated from utils/ntkernel-table.json inverted; do not edit.
// win32 GetLastError() error code -> std::errc; case 0 is ERROR_SUCCESS.
// Requires <cerrno> and ::std::errc.

	case 0u:
		return ::std::errc{};

#ifdef ENOSYS
	case 0x1u:
		return ::std::errc::function_not_supported;
#endif
#ifdef ENOENT
	case 0x2u:
		return ::std::errc::no_such_file_or_directory;
#endif
#ifdef ENOENT
	case 0x3u:
		return ::std::errc::no_such_file_or_directory;
#endif
#ifdef EMFILE
	case 0x4u:
		return ::std::errc::too_many_files_open;
#endif
#ifdef EACCES
	case 0x5u:
		return ::std::errc::permission_denied;
#endif
#ifdef EINVAL
	case 0x6u:
		return ::std::errc::invalid_argument;
#endif
#ifdef ENOMEM
	case 0x8u:
		return ::std::errc::not_enough_memory;
#endif
#ifdef ENOMEM
	case 0xeu:
		return ::std::errc::not_enough_memory;
#endif
#ifdef EXDEV
	case 0x11u:
		return ::std::errc::cross_device_link;
#endif
#ifdef EACCES
	case 0x13u:
		return ::std::errc::permission_denied;
#endif
#ifdef EAGAIN
	case 0x15u:
		return ::std::errc::resource_unavailable_try_again;
#endif
#ifdef EACCES
	case 0x20u:
		return ::std::errc::permission_denied;
#endif
#ifdef ENOLCK
	case 0x21u:
		return ::std::errc::no_lock_available;
#endif
#ifdef ENODEV
	case 0x37u:
		return ::std::errc::no_such_device;
#endif
#ifdef EACCES
	case 0x52u:
		return ::std::errc::permission_denied;
#endif
#ifdef ENOSPC
	case 0x70u:
		return ::std::errc::no_space_on_device;
#endif
#ifdef EINVAL
	case 0x7bu:
		return ::std::errc::invalid_argument;
#endif
#ifdef ENOTEMPTY
	case 0x91u:
		return ::std::errc::directory_not_empty;
#endif
#ifdef ETIMEDOUT
	case 0x92u:
		return ::std::errc::timed_out;
#endif
#ifdef EBUSY
	case 0xaau:
		return ::std::errc::device_or_resource_busy;
#endif
#ifdef EEXIST
	case 0xb7u:
		return ::std::errc::file_exists;
#endif
#ifdef EINVAL
	case 0x10bu:
		return ::std::errc::invalid_argument;
#endif
#ifdef ECANCELED
	case 0x3e3u:
		return ::std::errc::operation_canceled;
#endif
#ifdef EINPROGRESS
	case 0x3e5u:
		return ::std::errc::operation_in_progress;
#endif
#ifdef EACCES
	case 0x3e6u:
		return ::std::errc::permission_denied;
#endif
#ifdef EAGAIN
	case 0x4d5u:
		return ::std::errc::resource_unavailable_try_again;
#endif
#ifdef EBUSY
	case 0x961u:
		return ::std::errc::device_or_resource_busy;
#endif
#ifdef EBUSY
	case 0x964u:
		return ::std::errc::device_or_resource_busy;
#endif
