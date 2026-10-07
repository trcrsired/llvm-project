// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null -verify -DMAIN_CATCH_ALL %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null -verify -DMAIN_TWO_LEGACY %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null -verify -DFN_NOEXCEPT %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null -DMAIN_NESTED_OUTER_THROWS %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o /dev/null -DMAIN_CATCH_BODY %s
//
// CIR counterpart of clang/test/CodeGen/herbceptions-try-missing-catch-throws.cpp.
// The MAIN_WITH_CATCH_THROWS configuration is omitted: CIR does not implement
// try statements that interleave traditional and herbception catch handlers.

// A bare call to a 'throws' function inside a `try { }` body is only valid
// when the try has a `catch throws`/`catch fails` handler. A try whose
// handlers are all legacy `catch` clauses cannot consume the error, so the
// call is ill-formed — even in main(), whose trap-on-escape concession only
// applies to calls not enclosed by such a try.
// `catch` handler *bodies* are outside the try's protection, so a throws
// call there follows the ordinary rules (main() traps, throws functions
// propagate).

using size_t = __SIZE_TYPE__;

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  ~error() noexcept;
};
}

void throwing() throws;

// OK: a throws function auto-propagates the error; the legacy try is
// irrelevant to the herbception channel.
void ok_throws_legacy_try() throws {
  try {
    throwing();
  } catch (...) {
  }
}

#if defined(MAIN_CATCH_ALL)
int main() {
  try {
    throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  } catch (...) {
  }
}
#elif defined(MAIN_TWO_LEGACY)
int main() {
  try {
    throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  } catch (int) {
  } catch (float) {
  }
}
#elif defined(FN_NOEXCEPT)
void bad_noexcept_legacy_try() noexcept {
  try {
    throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  } catch (...) {
  }
}
int main() { throwing(); }
#elif defined(MAIN_NESTED_OUTER_THROWS)
// OK: the inner legacy try is bypassed; the outer catch throws claims the
// error (the inner try's cleanups still run on the way out).
int main() {
  try {
    try {
      throwing();
    } catch (int) {
    }
  } catch throws(::std::error e) {
    (void)e;
  }
}
#elif defined(MAIN_CATCH_BODY)
// OK: the throws call is in the catch handler body, outside the try's
// protection — the ordinary main() trap concession applies.
int main() {
  try {
  } catch (int) {
    throwing();
  }
}
#elif defined(MAIN_WITH_CATCH_THROWS)
// OK: legacy catches interleaved with catch throws.
int main() {
  try {
    throwing();
  } catch (int) {
  } catch (float) {
  } catch throws(::std::error e) {
    (void)e;
  }
}
#else
// OK: a bare throws call in main() traps at runtime by design.
int main() { throwing(); }
#endif
