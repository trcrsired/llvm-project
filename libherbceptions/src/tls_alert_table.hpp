// clang-format off
#define TLS_ALERT_MAX_SIZE 34

	case 0:
		return __tsc(u8"Close Notify");
	case 10:
		return __tsc(u8"Unexpected Message");
	case 20:
		return __tsc(u8"Bad Record MAC");
	case 21:
		return __tsc(u8"Decryption Failed");
	case 22:
		return __tsc(u8"Record Overflow");
	case 30:
		return __tsc(u8"Decompression Failure");
	case 40:
		return __tsc(u8"Handshake Failure");
	case 41:
		return __tsc(u8"No Certificate");
	case 42:
		return __tsc(u8"Bad Certificate");
	case 43:
		return __tsc(u8"Unsupported Certificate");
	case 44:
		return __tsc(u8"Certificate Revoked");
	case 45:
		return __tsc(u8"Certificate Expired");
	case 46:
		return __tsc(u8"Certificate Unknown");
	case 47:
		return __tsc(u8"Illegal Parameter");
	case 48:
		return __tsc(u8"Unknown CA");
	case 49:
		return __tsc(u8"Access Denied");
	case 50:
		return __tsc(u8"Decode Error");
	case 51:
		return __tsc(u8"Decrypt Error");
	case 60:
		return __tsc(u8"Export Restriction");
	case 70:
		return __tsc(u8"Protocol Version");
	case 71:
		return __tsc(u8"Insufficient Security");
	case 80:
		return __tsc(u8"Internal Error");
	case 86:
		return __tsc(u8"Inappropriate Fallback");
	case 90:
		return __tsc(u8"User Canceled");
	case 100:
		return __tsc(u8"No Renegotiation");
	case 109:
		return __tsc(u8"Missing Extension");
	case 110:
		return __tsc(u8"Unsupported Extension");
	case 111:
		return __tsc(u8"Certificate Unobtainable");
	case 112:
		return __tsc(u8"Unrecognized Name");
	case 113:
		return __tsc(u8"Bad Certificate Status Response");
	case 114:
		return __tsc(u8"Bad Certificate Hash Value");
	case 115:
		return __tsc(u8"Unknown PSK Identity");
	case 116:
		return __tsc(u8"Certificate Required");
	case 117:
		return __tsc(u8"General Error");
	case 120:
		return __tsc(u8"No Application Protocol");
	default:
		return __tsc(u8"Unknown");
// clang-format on
