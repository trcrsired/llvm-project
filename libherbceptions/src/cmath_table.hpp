// clang-format off
// Requires herbceptions/__details/cmath_errc.h (defines ::std::cmath_errc).
#define CMATH_ERRC_MAX_SIZE 32

	case ::std::cmath_errc::invalid:
		return __tsc(u8"Invalid floating point operation");
	case ::std::cmath_errc::divbyzero:
		return __tsc(u8"Floating point divide by zero");
	case ::std::cmath_errc::inexact:
		return __tsc(u8"Inexact floating point result");
	case ::std::cmath_errc::overflow:
		return __tsc(u8"Floating point overflow");
	case ::std::cmath_errc::underflow:
		return __tsc(u8"Floating point underflow");
	case ::std::cmath_errc::all_except:
		return __tsc(u8"All floating point exceptions");
	default:
		return __tsc(u8"Unknown");
// clang-format on
