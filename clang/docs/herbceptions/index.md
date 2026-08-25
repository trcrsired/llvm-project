# Herbceptions: Error Handling for C++

Herbceptions are a Clang extension that provides an error handling mechanism for
C++ using a `{T, i1}` return type to carry both the success value and a failure
discriminant. This is an alternative to C++ exceptions and is enabled with the
`-fherbceptions` flag.

## Overview

A function declared with `throws` or `fails{E}` has its return type transformed
into a `{T, i1}` record, where `T` is the original return type (or the error
type for `void` functions) and `i1` is the failure discriminant.

```cpp
int foo(int x) throws {
  if (x < 0) throw throws std::my_errc{x};
  return x + 1;
}
```

The function above is lowered to return `{{ptr, i64}, i1}` (because `int` is
smaller than the implicit `std::error` type, the payload is promoted to the
error type).

## Syntax

### throws specifier

```cpp
// Bare throws - uses implicit std::error type
int foo(int x) throws;

// throws with specific error type
int bar(int x) throws(std::my_errc);

// fails with specific error type
int baz(int x) fails{std::my_errc};
```

### throw throws expression

```cpp
void foo() throws {
  throw throws std::error{nullptr, 42};
}
```

### catch throws handler

```cpp
void callee() noexcept {
  try {
    bar();
  } catch throws(std::error e) {
    // Handle error
  }
}
```

### try(expr) auto-propagation

```cpp
int caller(int x) throws {
  return try(foo(x));  // Auto-propagates error on failure
}
```

### catch fails(expr)

```cpp
int foo(int x) {
  auto e = catch fails(bar(x));
  return !e.failed ? e.value * 2 : e.error;
}
```

## Language Rules

### Destructors

Destructors **cannot** have `throws` specifications:

```cpp
struct Bad {
  ~Bad() throws {} // Error: destructor cannot have a throws specifier
};
```

### Coroutines

Coroutines must not use `throws`:

```cpp
struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

Task make_task(int x) {  // OK - no throws
  co_return;
}
```

### Special Member Functions

Constructors, copy constructors, move constructors, and operator overloads can
have `throws` specifications:

```cpp
struct Foo {
  Foo(int x) throws : x(x) {}
  Foo(const Foo& other) throws : x(other.x) {}
  Foo(Foo&& other) throws : x(other.x) {}
  ~Foo() noexcept = default;  // Destructors cannot throw
};

struct Bar {
  Bar operator+(const Bar& other) throws { ... }
  int& operator[](int idx) throws { ... }
};
```

## Implementation

Herbceptions are implemented in both the classic CodeGen and ClangIR (CIR)
pipelines:

### Classic CodeGen
- `clang/lib/CodeGen/CGException.cpp` - Exception handling lowering
- `clang/lib/CodeGen/CGStmt.cpp` - Statement-level lowering
- `clang/lib/CodeGen/CGCall.cpp` - Call lowering

### ClangIR
- `clang/lib/CIR/CodeGen/CIRGenException.cpp` - Exception handling lowering
- `clang/lib/CIR/CodeGen/CIRGenCall.cpp` - Call lowering with herbception routing
- `clang/lib/CIR/CodeGen/CIRGenFunction.cpp` - Function-level herbception support
- `clang/lib/CIR/CodeGen/CIRGenTypes.cpp` - Type conversion for herbception signatures
- `clang/lib/CIR/CodeGen/CIRGenStmt.cpp` - Statement-level lowering
- `clang/lib/CIR/CodeGen/CIRGenExprScalar.cpp` - Scalar expression visitors
- `clang/lib/CIR/CodeGen/CIRGenExprAggregate.cpp` - Aggregate expression visitors

## Tests

Herbception tests are located in:
- `clang/test/CodeGen/herbception-*.cpp` - Classic codegen tests
- `clang/test/CIR/CodeGen/herbception-*.cpp` - ClangIR tests

## See Also

- [CIR Herbceptions](CIR/Herbceptions.md) - ClangIR-specific implementation details
