// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s

// Call-site handling for throws functions returning the shaped {E, i1}
// result: the error is checked on the discriminant, the success payload is
// coerced back to the declared return type before use, and a successful call
// does not end the enclosing function.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

void callee() throws;
void sink(void *);
int *getp() throws;

// A void throws call inside a throws function: on success the function must
// continue to the next statement, not return early.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z6callerDrv()
// CHECK:         call { { ptr, i64 }, i8 } @_Z6calleeDrv()
// CHECK:         call void @_Z4sinkPv(ptr noundef null)
// CHECK:         ret { { ptr, i64 }, i8 }
void caller() throws {
  callee();
  sink(nullptr);
}

// The shaped payload slot is sized for the larger of the declared return
// type and the error type; a pointer result must be read back as a pointer
// before scalar operations consume it.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z7compareDrv()
// CHECK:         call { { ptr, i64 }, i8 } @_Z4getpDrv()
// CHECK:         icmp eq ptr
void compare() throws {
  if (getp() == nullptr)
    return;
}

// A bare 'throw throws' inside a 'catch throws' handler rethrows the error
// the handler caught: it is loaded back out of the handler's error slot and
// returned with the failure discriminant set.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z7rethrowDrv()
// CHECK:         store i8 1
// CHECK:         ret { { ptr, i64 }, i8 }
void rethrow() throws {
  try {
    callee();
  } catch throws(std::error) {
    throw throws;
  }
}
