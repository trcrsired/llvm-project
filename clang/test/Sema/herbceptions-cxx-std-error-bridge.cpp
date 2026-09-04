// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DVALID -fsyntax-only %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DVALID -DCONST_DOMAIN -fsyntax-only %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DPACKED -fsyntax-only -verify=invalid %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DEXTRA_FIELD -fsyntax-only -verify=invalid %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DBAD_DOMAIN -fsyntax-only -verify=invalid %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DBAD_CODE -fsyntax-only -verify=invalid %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -DOVERALIGNED -fsyntax-only -verify=invalid %s

// `cxx_std_error` is the sole representation-level typed-to-basic bridge. Its
// spelling alone is insufficient: the compiler must prove the exact global
// two-field target ABI before forwarding its active bytes as std::error.

namespace std {
template <class T> struct error_domain;
}

#if defined(PACKED)
struct __attribute__((packed)) cxx_std_error {
  void *domain;
  __SIZE_TYPE__ code;
};
#elif defined(EXTRA_FIELD)
struct cxx_std_error {
  void *domain;
  __SIZE_TYPE__ code;
  unsigned extra;
};
#elif defined(BAD_DOMAIN)
struct cxx_std_error {
  void (*domain)();
  __SIZE_TYPE__ code;
};
#elif defined(BAD_CODE)
struct cxx_std_error {
  void *domain;
  unsigned code;
};
#elif defined(OVERALIGNED)
struct alignas(64) cxx_std_error {
  void *domain;
  __SIZE_TYPE__ code;
};
#else
struct cxx_std_error {
#if defined(CONST_DOMAIN)
  void const *domain;
#else
  void *domain;
#endif
  __SIZE_TYPE__ code;
};
#endif

int bridge_source() return_failure{cxx_std_error};

int bridge_forward() throws {
  return bridge_source(); // invalid-error {{no std::error_domain specialization}}
}
