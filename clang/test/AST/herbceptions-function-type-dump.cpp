// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions \
// RUN:   -ast-dump -ast-dump-filter=typed_alias %s | FileCheck %s

using typed_alias = int() return_failure{long};

// A typed channel is neither a basic noexcept nor an untyped throws channel.
// CHECK: FunctionProtoType {{.*}} 'int () fails{long}' exceptionspec_throws_typed
// CHECK-NEXT: {{.*}}Exceptions: 'long'
