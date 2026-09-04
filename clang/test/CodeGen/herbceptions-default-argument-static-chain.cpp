// REQUIRES: native, target={{x86_64-.*linux.*}}
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -emit-llvm -o %t.ll %s
// RUN: FileCheck %s --input-file=%t.ll
// RUN: opt -passes=verify -disable-output %t.ll
// RUN: %clangxx -std=c++20 -fherbceptions -fno-exceptions -O0 %s -o %t.o0
// RUN: %t.o0
// RUN: %clangxx -std=c++20 -fherbceptions -fno-exceptions -O2 %s -o %t.o2
// RUN: %t.o2

// Default arguments retain their raw callable effect until a use site chooses
// the failure destination. The static-chain builtin invokes the AST-shaped
// EmitCall overload directly with no output-instruction slot. Two absent
// slots must not be mistaken for an active try/catch wrapper owning the call:
// on failure, control must leave the argument before consume_default executes.

bool fail_source;
int source_calls;
int consumer_calls;
int consumed_value;

int default_source() return_failure{int} {
  ++source_calls;
  if (fail_source)
    return_failure 23;
  return 41;
}

void consume_default(
    int value = __builtin_call_with_static_chain(default_source(), (void *)0)) {
  ++consumer_calls;
  consumed_value = value;
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}invoke_defaultv(
// CHECK: call {{.*}} @_Z{{[0-9]+}}default_sourcev(ptr nest
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1
// CHECK: call void @_Z{{[0-9]+}}consume_defaulti(
int invoke_default() return_failure{int} {
  consume_default();
  return 99;
}

int main() {
  auto success = catch return_failure(invoke_default());
  if (success.failed || success.value != 99 || source_calls != 1 ||
      consumer_calls != 1 || consumed_value != 41)
    return 1;

  fail_source = true;
  source_calls = 0;
  consumer_calls = 0;
  consumed_value = 0;
  auto failure = catch return_failure(invoke_default());
  if (!failure.failed || failure.error != 23 || source_calls != 1 ||
      consumer_calls != 0 || consumed_value != 0)
    return 2;
}
