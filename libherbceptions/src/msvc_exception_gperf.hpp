enum class msvc_exception_kind {
  unknown,
  msvc_system_error,
  msvc_logic_error,
  msvc_domain_error,
  msvc_invalid_argument,
  msvc_length_error,
  msvc_out_of_range,
  msvc_runtime_error,
  msvc_range_error,
  msvc_overflow_error,
  msvc_underflow_error,
  msvc_bad_alloc
};

struct msvc_exception_entry {
  char const *name;
  msvc_exception_kind value;
};

#define TOTAL_KEYWORDS 11
#define MIN_WORD_LENGTH 19
#define MAX_WORD_LENGTH 26
#define MIN_HASH_VALUE 19
#define MAX_HASH_VALUE 37
/* maximum key range = 19, duplicates = 0 */

class Perfect_Hash {
private:
  static inline ::std::uint_least32_t msvc_exception_hash(char const *str,
                                                          size_t len);

public:
  static inline msvc_exception_entry const *
  msvc_exception_lookup(char const *str, size_t len);
};

inline ::std::uint_least32_t Perfect_Hash::msvc_exception_hash(char const *str,
                                                               size_t len) {
  static constexpr unsigned char asso_values[] = {
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 0,  38, 15, 38, 38, 38, 38, 5,  38, 38,
      5,  38, 38, 0,  38, 38, 0,  10, 38, 0,  38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
      38, 38, 38, 38};
  return len + asso_values[static_cast<unsigned char>(str[4])];
}
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 6) > 4) ||              \
    (defined __clang__ && __clang_major__ >= 3)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
inline constexpr msvc_exception_entry wordlist[] = {
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
    {""},
#line 43 "msvc_exceptions_hash.gperf"
    {".?AVbad_alloc@std@@", msvc_exception_kind::msvc_bad_alloc},
    {""},
#line 40 "msvc_exceptions_hash.gperf"
    {".?AVrange_error@std@@", msvc_exception_kind::msvc_range_error},
#line 38 "msvc_exceptions_hash.gperf"
    {".?AVout_of_range@std@@", msvc_exception_kind::msvc_out_of_range},
#line 39 "msvc_exceptions_hash.gperf"
    {".?AVruntime_error@std@@", msvc_exception_kind::msvc_runtime_error},
#line 41 "msvc_exceptions_hash.gperf"
    {".?AVoverflow_error@std@@", msvc_exception_kind::msvc_overflow_error},
#line 42 "msvc_exceptions_hash.gperf"
    {".?AVunderflow_error@std@@", msvc_exception_kind::msvc_underflow_error},
#line 34 "msvc_exceptions_hash.gperf"
    {".?AVlogic_error@std@@", msvc_exception_kind::msvc_logic_error},
#line 37 "msvc_exceptions_hash.gperf"
    {".?AVlength_error@std@@", msvc_exception_kind::msvc_length_error},
    {""},
    {""},
    {""},
#line 36 "msvc_exceptions_hash.gperf"
    {".?AVinvalid_argument@std@@", msvc_exception_kind::msvc_invalid_argument},
#line 33 "msvc_exceptions_hash.gperf"
    {".?AVsystem_error@std@@", msvc_exception_kind::msvc_system_error},
    {""},
    {""},
    {""},
    {""},
#line 35 "msvc_exceptions_hash.gperf"
    {".?AVdomain_error@std@@", msvc_exception_kind::msvc_domain_error}};
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 6) > 4) ||              \
    (defined __clang__ && __clang_major__ >= 3)
#pragma GCC diagnostic pop
#endif

inline msvc_exception_entry const *
Perfect_Hash::msvc_exception_lookup(char const *str, size_t len) {

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
    ::std::uint_least32_t key = msvc_exception_hash(str, len);

    if (key <= MAX_HASH_VALUE) {
      char const *s = wordlist[key].name;

      if (*str == *s && !strncmp(str + 1, s + 1, len - 1) && s[len] == '\0')
        return &wordlist[key];
    }
  }
  return static_cast<struct msvc_exception_entry *>(0);
}
