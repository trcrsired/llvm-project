// clang-format off
// Requires <cerrno> and herbceptions/__details/wine.h.

	case ::std::wine_errc::success:
		return ::std::errc{};
#if defined(EPERM) && (!defined(EACCES) || (EPERM != EACCES))
	case ::std::wine_errc::operation_not_permitted:
		return ::std::errc::operation_not_permitted;
#endif
#ifdef ENOENT
	case ::std::wine_errc::no_such_file_or_directory:
		return ::std::errc::no_such_file_or_directory;
#endif
#ifdef ESRCH
	case ::std::wine_errc::no_such_process:
		return ::std::errc::no_such_process;
#endif
#ifdef EINTR
	case ::std::wine_errc::interrupted:
		return ::std::errc::interrupted;
#endif
#ifdef EIO
	case ::std::wine_errc::io_error:
		return ::std::errc::io_error;
#endif
#if defined(ENXIO) && (!defined(ENODEV) || (ENXIO != ENODEV))
	case ::std::wine_errc::no_such_device_or_address:
		return ::std::errc::no_such_device_or_address;
#endif
#ifdef E2BIG
	case ::std::wine_errc::argument_list_too_long:
		return ::std::errc::argument_list_too_long;
#endif
#ifdef ENOEXEC
	case ::std::wine_errc::executable_format_error:
		return ::std::errc::executable_format_error;
#endif
#ifdef EBADF
	case ::std::wine_errc::bad_file_descriptor:
		return ::std::errc::bad_file_descriptor;
#endif
#ifdef ECHILD
	case ::std::wine_errc::no_child_process:
		return ::std::errc::no_child_process;
#endif
#ifdef EAGAIN
	case ::std::wine_errc::resource_unavailable_try_again:
		return ::std::errc::resource_unavailable_try_again;
#endif
#ifdef ENOMEM
	case ::std::wine_errc::not_enough_memory:
		return ::std::errc::not_enough_memory;
#endif
#ifdef EFAULT
	case ::std::wine_errc::bad_address:
		return ::std::errc::bad_address;
#endif
#ifdef EBUSY
	case ::std::wine_errc::device_or_resource_busy:
		return ::std::errc::device_or_resource_busy;
#endif
#ifdef EEXIST
	case ::std::wine_errc::file_exists:
		return ::std::errc::file_exists;
#endif
#ifdef EXDEV
	case ::std::wine_errc::cross_device_link:
		return ::std::errc::cross_device_link;
#endif
#ifdef ENODEV
	case ::std::wine_errc::no_such_device:
		return ::std::errc::no_such_device;
#endif
#ifdef ENOTDIR
	case ::std::wine_errc::not_a_directory:
		return ::std::errc::not_a_directory;
#endif
#ifdef EISDIR
	case ::std::wine_errc::is_a_directory:
		return ::std::errc::is_a_directory;
#endif
#ifdef EINVAL
	case ::std::wine_errc::invalid_argument:
		return ::std::errc::invalid_argument;
#endif
#ifdef ENFILE
	case ::std::wine_errc::too_many_files_open_in_system:
		return ::std::errc::too_many_files_open_in_system;
#endif
#ifdef EMFILE
	case ::std::wine_errc::too_many_files_open:
		return ::std::errc::too_many_files_open;
#endif
#ifdef ENOTTY
	case ::std::wine_errc::inappropriate_io_control_operation:
		return ::std::errc::inappropriate_io_control_operation;
#endif
#ifdef ETXTBSY
	case ::std::wine_errc::text_file_busy:
		return ::std::errc::text_file_busy;
#endif
#ifdef EFBIG
	case ::std::wine_errc::file_too_large:
		return ::std::errc::file_too_large;
#endif
#ifdef EHOSTUNREACH
	case ::std::wine_errc::host_unreachable:
		return ::std::errc::host_unreachable;
#endif
#ifdef ENOSPC
	case ::std::wine_errc::no_space_on_device:
		return ::std::errc::no_space_on_device;
#endif
#ifdef ENOTSUP
	case ::std::wine_errc::operation_not_supported:
		return ::std::errc::operation_not_supported;
#endif
#ifdef ESPIPE
	case ::std::wine_errc::invalid_seek:
		return ::std::errc::invalid_seek;
#endif
#ifdef EROFS
	case ::std::wine_errc::read_only_file_system:
		return ::std::errc::read_only_file_system;
#endif
#ifdef EMLINK
	case ::std::wine_errc::too_many_links:
		return ::std::errc::too_many_links;
#endif
#ifdef EPIPE
	case ::std::wine_errc::broken_pipe:
		return ::std::errc::broken_pipe;
#endif
#ifdef EDOM
	case ::std::wine_errc::argument_out_of_domain:
		return ::std::errc::argument_out_of_domain;
#endif
#ifdef ERANGE
	case ::std::wine_errc::result_out_of_range:
		return ::std::errc::result_out_of_range;
#endif
#ifdef ENOMSG
	case ::std::wine_errc::no_message:
		return ::std::errc::no_message;
#endif
#ifdef EIDRM
	case ::std::wine_errc::identifier_removed:
		return ::std::errc::identifier_removed;
#endif
#ifdef EILSEQ
	case ::std::wine_errc::illegal_byte_sequence:
		return ::std::errc::illegal_byte_sequence;
#endif
#ifdef EDEADLK
	case ::std::wine_errc::resource_deadlock_would_occur:
		return ::std::errc::resource_deadlock_would_occur;
#endif
#ifdef ENETUNREACH
	case ::std::wine_errc::network_unreachable:
		return ::std::errc::network_unreachable;
#endif
#ifdef ENOLCK
	case ::std::wine_errc::no_lock_available:
		return ::std::errc::no_lock_available;
#endif
#ifdef ENOLINK
	case ::std::wine_errc::no_link:
		return ::std::errc::no_link;
#endif
#ifdef EPROTO
	case ::std::wine_errc::protocol_error:
		return ::std::errc::protocol_error;
#endif
#ifdef EPROTONOSUPPORT
	case ::std::wine_errc::protocol_not_supported:
		return ::std::errc::protocol_not_supported;
#endif
#ifdef EBADMSG
	case ::std::wine_errc::bad_message:
		return ::std::errc::bad_message;
#endif
#ifdef ENOSYS
	case ::std::wine_errc::function_not_supported:
		return ::std::errc::function_not_supported;
#endif
#ifdef ENOTEMPTY
	case ::std::wine_errc::directory_not_empty:
		return ::std::errc::directory_not_empty;
#endif
#ifdef ENAMETOOLONG
	case ::std::wine_errc::filename_too_long:
		return ::std::errc::filename_too_long;
#endif
#ifdef ELOOP
	case ::std::wine_errc::too_many_symbolic_link_levels:
		return ::std::errc::too_many_symbolic_link_levels;
#endif
#ifdef ENOBUFS
	case ::std::wine_errc::no_buffer_space:
		return ::std::errc::no_buffer_space;
#endif
#ifdef EAFNOSUPPORT
	case ::std::wine_errc::address_family_not_supported:
		return ::std::errc::address_family_not_supported;
#endif
#ifdef EPROTOTYPE
	case ::std::wine_errc::wrong_protocol_type:
		return ::std::errc::wrong_protocol_type;
#endif
#ifdef ENOTSOCK
	case ::std::wine_errc::not_a_socket:
		return ::std::errc::not_a_socket;
#endif
#ifdef ENOPROTOOPT
	case ::std::wine_errc::no_protocol_option:
		return ::std::errc::no_protocol_option;
#endif
#ifdef ECONNREFUSED
	case ::std::wine_errc::connection_refused:
		return ::std::errc::connection_refused;
#endif
#ifdef ECONNRESET
	case ::std::wine_errc::connection_reset:
		return ::std::errc::connection_reset;
#endif
#ifdef EADDRINUSE
	case ::std::wine_errc::address_in_use:
		return ::std::errc::address_in_use;
#endif
#ifdef EADDRNOTAVAIL
	case ::std::wine_errc::address_not_available:
		return ::std::errc::address_not_available;
#endif
#ifdef ECONNABORTED
	case ::std::wine_errc::connection_aborted:
		return ::std::errc::connection_aborted;
#endif
#if (defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN)))
	case ::std::wine_errc::would_block:
		return static_cast<::std::errc>(EAGAIN);
#endif
#ifdef ENOTCONN
	case ::std::wine_errc::not_connected:
		return ::std::errc::not_connected;
#endif
#ifdef EISCONN
	case ::std::wine_errc::already_connected:
		return ::std::errc::already_connected;
#endif
#ifdef ECANCELED
	case ::std::wine_errc::operation_canceled:
		return ::std::errc::operation_canceled;
#endif
#ifdef ENOTRECOVERABLE
	case ::std::wine_errc::state_not_recoverable:
		return ::std::errc::state_not_recoverable;
#endif
#ifdef EOWNERDEAD
	case ::std::wine_errc::owner_dead:
		return ::std::errc::owner_dead;
#endif
#ifdef EOVERFLOW
	case ::std::wine_errc::value_too_large:
		return ::std::errc::value_too_large;
#endif
#ifdef EMSGSIZE
	case ::std::wine_errc::message_size:
		return ::std::errc::message_size;
#endif
#ifdef ETIMEDOUT
	case ::std::wine_errc::timed_out:
		return ::std::errc::timed_out;
#endif
#ifdef ESTALE
	case ::std::wine_errc::stale_file_handle:
		return static_cast<::std::errc>(ESTALE);
#endif
	default:
		return ::std::errc::io_error;
// clang-format on
