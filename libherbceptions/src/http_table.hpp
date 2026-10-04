// clang-format off
#define HTTP_ERRC_MAX_SIZE 32

	case 100:
		return __tsc(u8"Continue");
	case 101:
		return __tsc(u8"Switching Protocols");
	case 102:
		return __tsc(u8"Processing");
	case 103:
		return __tsc(u8"Early Hints");
	case 200:
		return __tsc(u8"OK");
	case 201:
		return __tsc(u8"Created");
	case 202:
		return __tsc(u8"Accepted");
	case 203:
		return __tsc(u8"Non-Authoritative Information");
	case 204:
		return __tsc(u8"No Content");
	case 205:
		return __tsc(u8"Reset Content");
	case 206:
		return __tsc(u8"Partial Content");
	case 207:
		return __tsc(u8"Multi-Status");
	case 208:
		return __tsc(u8"Already Reported");
	case 226:
		return __tsc(u8"IM Used");
	case 300:
		return __tsc(u8"Multiple Choices");
	case 301:
		return __tsc(u8"Moved Permanently");
	case 302:
		return __tsc(u8"Found");
	case 303:
		return __tsc(u8"See Other");
	case 304:
		return __tsc(u8"Not Modified");
	case 305:
		return __tsc(u8"Use Proxy");
	case 307:
		return __tsc(u8"Temporary Redirect");
	case 308:
		return __tsc(u8"Permanent Redirect");
	case 400:
		return __tsc(u8"Bad Request");
	case 401:
		return __tsc(u8"Unauthorized");
	case 402:
		return __tsc(u8"Payment Required");
	case 403:
		return __tsc(u8"Forbidden");
	case 404:
		return __tsc(u8"Not Found");
	case 405:
		return __tsc(u8"Method Not Allowed");
	case 406:
		return __tsc(u8"Not Acceptable");
	case 407:
		return __tsc(u8"Proxy Authentication Required");
	case 408:
		return __tsc(u8"Request Timeout");
	case 409:
		return __tsc(u8"Conflict");
	case 410:
		return __tsc(u8"Gone");
	case 411:
		return __tsc(u8"Length Required");
	case 412:
		return __tsc(u8"Precondition Failed");
	case 413:
		return __tsc(u8"Content Too Large");
	case 414:
		return __tsc(u8"URI Too Long");
	case 415:
		return __tsc(u8"Unsupported Media Type");
	case 416:
		return __tsc(u8"Range Not Satisfiable");
	case 417:
		return __tsc(u8"Expectation Failed");
	case 418:
		return __tsc(u8"I'm a Teapot");
	case 421:
		return __tsc(u8"Misdirected Request");
	case 422:
		return __tsc(u8"Unprocessable Content");
	case 423:
		return __tsc(u8"Locked");
	case 424:
		return __tsc(u8"Failed Dependency");
	case 425:
		return __tsc(u8"Too Early");
	case 426:
		return __tsc(u8"Upgrade Required");
	case 428:
		return __tsc(u8"Precondition Required");
	case 429:
		return __tsc(u8"Too Many Requests");
	case 431:
		return __tsc(u8"Request Header Fields Too Large");
	case 451:
		return __tsc(u8"Unavailable For Legal Reasons");
	case 500:
		return __tsc(u8"Internal Server Error");
	case 501:
		return __tsc(u8"Not Implemented");
	case 502:
		return __tsc(u8"Bad Gateway");
	case 503:
		return __tsc(u8"Service Unavailable");
	case 504:
		return __tsc(u8"Gateway Timeout");
	case 505:
		return __tsc(u8"HTTP Version Not Supported");
	case 506:
		return __tsc(u8"Variant Also Negotiates");
	case 507:
		return __tsc(u8"Insufficient Storage");
	case 508:
		return __tsc(u8"Loop Detected");
	case 510:
		return __tsc(u8"Not Extended");
	case 511:
		return __tsc(u8"Network Authentication Required");
	default:
		return __tsc(u8"Unknown");
// clang-format on
