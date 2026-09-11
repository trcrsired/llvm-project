// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -fno-discard-value-names -S -emit-llvm -o - %s | FileCheck %s

// Destructor ownership across the paths out of a `catch throws` handler.
//
// The error in a handler is an owned object: it is destroyed when the handler
// finishes with it. The exception is a rethrow, which moves the error out --
// there is then nothing left in the handler's copy to destroy, and destroying
// it anyway would run a destructor on an object whose value now lives
// elsewhere.
//
// There are three ways a handler finishes with its error, and they differ:
//
//   throw throws;            moves the caught error out  -> no destructor
//   throw throws e;          copies it into a new error  -> destructor runs
//   throw throws <new value> builds a fresh error        -> destructor runs
//
// The last two build a new error out of their operand through that operand's
// error_domain, leaving the caught error untouched and still owned here.
//
// The decision cannot be made once per handler, because a single handler can
// take any of these on different paths (mixed below takes two of them), so it
// is a per-path flag: the destructor is made conditional on it and the rethrow
// paths clear it. -fno-discard-value-names is used so the checks can name the
// flag, which is the thing under test.

namespace std {

struct error_domain_singleton {};

struct error {
  void *d;
  __SIZE_TYPE__ c;
  ~error();
};

enum class errc {
  success = 0,
  io_error = 5,
  network_down = 6,
};

inline bool operator==(const error &e, errc v) {
  return e.c == (__SIZE_TYPE__)v;
}

template <class T> class error_domain;

template <> class error_domain<errc> {
public:
  static const error_domain_singleton *domain() noexcept;
  static __SIZE_TYPE__ code(errc e) noexcept { return (__SIZE_TYPE__)e; }
};

} // namespace std

void foo(int) throws;

// A bare `throw throws;` is the rethrow: the flag starts set, the rethrow path
// clears it, and the destructor is conditional on it.
//
// CHECK-LABEL: define {{.*}} @_Z4barei(
// CHECK:         catch.throws:
// CHECK:         store i1 true, ptr %herb.catchvar.alive
// CHECK:         store i1 false, ptr %herb.catchvar.alive
// CHECK:       herb.catchvar.dtor:
// CHECK:         call void @_ZNSt5errorD{{[12]}}Ev
void bare(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    (void)e;
    throw throws;
  }
}

// `throw throws e` copies the caught error into a new one, so the caught error
// is still owned here: the flag is set and never cleared, and the destructor
// runs. This is the whole difference from the bare form.
//
// CHECK-LABEL: define {{.*}} @_Z4copyi(
// CHECK:         catch.throws:
// CHECK:         store i1 true, ptr %herb.catchvar.alive
// CHECK-NOT:     store i1 false, ptr %herb.catchvar.alive
// CHECK:         call void @_ZNSt5errorD{{[12]}}Ev
void copy(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    throw throws e;
  }
}

// A fresh error built out of an errc goes through that domain's code()/domain()
// and leaves the caught error owned here, exactly like the copy above.
//
// CHECK-LABEL: define {{.*}} @_Z5freshi(
// CHECK:         catch.throws:
// CHECK:         store i1 true, ptr %herb.catchvar.alive
// CHECK-NOT:     store i1 false, ptr %herb.catchvar.alive
// CHECK:         call {{.*}} @_ZNSt12error_domainISt4errcE6domainEv
// CHECK:         call {{.*}} @_ZNSt12error_domainISt4errcE4codeES0_
// CHECK:         call void @_ZNSt5errorD{{[12]}}Ev
void fresh(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    (void)e;
    throw throws std::errc::network_down;
  }
}

// The same handler taking two of these on different paths. A per-handler
// decision would either destroy the moved-out error or leak the one it still
// owns; the flag makes each path correct on its own.
//
// CHECK-LABEL: define {{.*}} @_Z5mixedi(
// CHECK:         catch.throws:
// CHECK:         store i1 true, ptr %herb.catchvar.alive
// CHECK:         store i1 false, ptr %herb.catchvar.alive
// CHECK:       herb.catchvar.dtor:
// CHECK:         call void @_ZNSt5errorD{{[12]}}Ev
void mixed(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    if (e == std::errc::io_error)
      throw throws std::errc::network_down;
    throw throws;
  }
}
