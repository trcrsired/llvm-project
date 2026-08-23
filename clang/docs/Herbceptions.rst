.. _herbceptions:

==========================================================
Herbceptions: Zero-Overhead Deterministic Errors
==========================================================

.. contents::
   :local:

.. note::

   This is an **experimental** extension under active development. The
   implementation is incomplete, the ABI is not stable, and the design may
   change. Enable it with ``-fherbceptions``.

Overview
========

Herbceptions (after Herb Sutter's `P0709R4: Zero-overhead deterministic
exceptions <https://wg21.link/p0709r4>`_) replace the traditional two-phase
C++ exception model with a **deterministic error channel**. A function that
can fail declares *that it can fail* with a ``throws`` (C++) or ``fails{E}``
(C and C++) specifier, and the error is returned through the normal return
path, discriminated by a boolean flag carried next to the value.

The two models are orthogonal:

* **Traditional EH** (``throw`` / ``try`` / ``catch``): requires
  ``-fexceptions`` and uses the unwinder, landing pads, and a personality
  function. The unwind is asynchronous; destructors run during phase-2
  unwinding.
* **Herbceptions** (``throw throws`` / ``try(expr)`` / ``catch fails``):
  enabled by ``-fherbceptions``. Errors are returned like ordinary values;
  there is no unwinder, no landing pad, no personality function, and no
  LSDA. Destructors run on the normal scope-exit path.

The two flags are independent: ``-fherbceptions`` controls the deterministic
error channel and does *not* require ``-fexceptions``.

Why not traditional exceptions?
-------------------------------

Traditional C++ exceptions were rejected as the basis of this design because
of the implementation and runtime costs that come with a two-phase,
stack-unwinding model:

* **Implementation difficulty.** A correct two-phase unwinder (search phase,
  then unwind phase), the LSDA encodings, and the personality functions
  (Itanium ``__gxx_personality_v0``, MSVC ``__CxxFrameHandler3``, Wasm) are
  large, fragile systems. A deterministic error channel needs none of them.
* **Performance.** ``throw`` is not zero-overhead: even when an exception is
  never thrown, code with exception handling must compute exception ranges,
  and throwing itself must walk a live-call stack. With herbceptions the
  success path is just a call plus a branch on a flag.
* **Binary bloat.** Landing pads, type tables, and unwind tables add
  significant object-code size.
* **Not freestanding-friendly.** Exception handling depends on runtime ABI
  support (``<cxxabi.h>``, personality routines, ``__cxa_*``). The
  deterministic channel is a plain calling-convention feature and works in
  freestanding environments.

Language-level design
=====================

Function specifiers
-------------------

Two specifiers exist and cannot coexist on the same function:

.. code-block:: cpp

   // C++ only. Implicit error type std::error.
   T foo() throws;

   // C++ and C. Explicit error type E (trivially copyable).
   T foo() fails{E};

Semantically:

.. code-block:: cpp

   T foo() throws;                    // noexcept(true) + throws channel
   T foo() fails{E};                  // noexcept(true) + fails{E} channel
   T foo() noexcept(false) fails{E};  // explicit override: allows both

* ``throws`` and ``fails{E}`` are part of the *canonical function type* and
  change the function ABI (the return type is lowered to ``{T, i1}``). This
  is unlike ``noexcept`` since C++17, which is not part of the canonical
  type.
* ``throws(false)`` means the function cannot fail: it is plain ``noexcept``.
* A ``throws`` function implies ``noexcept(true)``; combining it with
  ``noexcept(false)`` is a compile-time error.
* The default (implicit) ``fails{E}`` implies ``noexcept(true)``: any
  traditional C++ exception that escapes it calls ``std::terminate`` (it has
  a terminate landing pad, exactly like a ``noexcept`` function). Only an
  explicit ``fails{E} noexcept(false)`` lets traditional exceptions
  propagate.

Function pointers
`````````````````

Because the specifier changes the canonical type, function pointers with
``throws``/``fails`` are distinct types. There are **no implicit
conversions** in either direction to/from plain function pointers, and a
virtual override must use the same specifier as the base.

Error domains
`````````````

An error *type* must be registered with a ``std::error_domain``
specialization exposing at least a non-null ``domain()`` singleton pointer
and a ``code(E)`` projection:

.. code-block:: cpp

   namespace std {
   enum class my_errc : unsigned { ok = 0, bad = 1 };
   template <> struct error_domain<my_errc> {
     static constexpr error_domain_singleton const *domain() noexcept;
     static constexpr unsigned long code(my_errc e) noexcept {
       return static_cast<unsigned long>(e);
     }
   };
   }

``std::errc`` (the POSIX domain), ``std::win32_errc``, ``std::nt_errc``,
``std::com_errc`` and ``std::wine_errc`` are provided by the ``libherbceptions``
runtime. ``domain()`` must never return ``nullptr``: the fabricated
``std::error`` dereferences the domain pointer in its destructor.

Returning an error
------------------

A ``throws`` function returns an error with ``throw throws expr``
(C++ only):

.. code-block:: cpp

   int read_file(const char *path) throws {
     FILE *f = fopen(path, "rb");
     if (!f) throw throws std::errc(errno);
     return fileno(f);
   }

A ``fails{E}`` function returns an error with ``failure(expr)`` (C and C++),
or ``throw throws expr`` where the compiler can convert ``E`` to
``std::error`` (C++):

.. code-block:: c

   int divide(int a, int b) fails{int} {
     if (b == 0) return failure(42);
     return a / b;
   }

The ``fails{E}`` error type must be **trivially copyable**; the error value
flows through the ``{T, i1}`` ABI slot by value.

Calling a function that can fail
--------------------------------

**C++** -- a bare call to a ``throws``/``fails`` function inside a function
that itself has a ``throws``/``fails`` specifier **auto-propagates** the
error; no wrapper is required:

.. code-block:: cpp

   int process(int x) throws {
     int fd = open_wrapped(x);        // auto-propagates on failure
     return fd;
   }

The auto-propagation is suppressed while parsing the operand of an
explicit ``try(expr)`` or ``catch fails(expr)``.

**C** -- calling a ``fails{E}`` function without an explicit wrapper is a
compile-time error. The error must be handled with ``try(expr)`` or
``catch fails(expr)``:

.. code-block:: c

   // error: calling function with 'fails{...}' specifier requires
   //        'try()' or 'catch fails()' wrapper
   int x = some_fails_func();

   int x = try(some_fails_func());                    // auto-propagate
   struct { union { int value; int error; }; bool failed; }
     e = catch fails(some_fails_func());               // inspect

Explicit handling
-----------------

``try(expr)`` -- evaluates ``expr`` (a call to a ``throws``/``fails``
function); on failure it auto-propagates the error; on success it yields the
success value:

.. code-block:: cpp

   int foo(int x) throws {
     return try(bar(x)) + 1;   // if bar fails, foo fails with bar's error
   }

``catch fails(expr)`` -- evaluates ``expr`` and produces the N2289 aggregate
``struct { union { T value; E error; }; bool failed; }``. ``value`` and
``error`` are accessible through the anonymous union; ``failed`` is false on
success and true on error:

.. code-block:: cpp

   auto e = catch fails(bar(x));
   if (e.failed) {
     handle(e.error);
   } else {
     use(e.value);
   }

Catching errors with a block
----------------------------

``catch throws(E e) { ... }`` and ``catch fails(E e) { ... }`` provide
block-based handlers. A bare call to a ``throws``/``fails`` function inside
the ``try`` block routes the error value to the handler (which binds the
exception variable) instead of being dropped:

.. code-block:: cpp

   try {
     try foo();          // foo() throws
   } catch throws(std::error e) {
     if (e == std::errc::no_such_file_or_directory) { ... }
   }

Convertibility between specifiers
`````````````````````````````````

* Calling a ``fails{E}`` function from a ``throws`` function requires ``E``
  to be convertible to ``std::error`` (i.e. a ``std::error_domain<E>``
  exists).
* Calling a ``throws`` function from a ``fails{E}`` function requires
  ``std::error`` to be convertible to ``E``.

``noexcept`` boundary
---------------------

A herbception error must never silently escape a ``noexcept(true)``
function. Calling a ``throws(true)`` function without handling it from a
``noexcept(true)`` function is a compile-time error, and ``try foo()``
(which propagates) is likewise rejected there; use ``catch throws(...)``
instead. ``int main()`` is a special case: an unhandled error terminates via
``__builtin_trap()``.

Traditional exceptions
----------------------

A ``throws`` function implicitly **converts any legacy C++ exception that
escapes it** (thrown by a ``noexcept(false)`` callee) into a fabricated
``std::error`` on the herbception channel, exactly like a
``catch throws(std::error)`` handler does inside a ``try`` block. The
fabricated error is ``{ error_domain<std::cxa_exception_code>::domain(),
__cxa_get_exception_ptr(...) }``. Destructors still run normally (during
unwinding, since ``throws`` calls are plain calls/invokes). A missing
``error_domain<std::cxa_exception_code>`` specialization silently disables
the conversion (the exception propagates as before).

Templates and concepts
----------------------

``try(expr)`` and ``catch fails(expr)`` accept dependent calls. Inside a
template the check is deferred to instantiation, and the herbception flag on
``throw throws`` is preserved when the expression is rebuilt, so
instantiated templates behave correctly. Constrained templates (concepts)
work as usual.

Constexpr
---------

``fails{E}`` functions, ``try(expr)`` auto-propagation and
``catch fails(expr)`` are usable in constant expressions:

.. code-block:: cpp

   constexpr int f(int x) fails{int} {
     if (x == 0) return failure(42);
     return 2 * x;
   }
   constexpr int g(int x) fails{int} { return try(f(x)); }
   static_assert(g(3) == 6);
   static_assert(g(0) == 42);

Coroutines
----------

A coroutine may be declared ``throws``. The ramp function returns
``{Task, i1}`` with the ``throws`` attribute, and the discriminant is kept in
a stack slot (not the coroutine frame, which may be deallocated before the
ramp reads it back). ``catch fails(coro())`` works on coroutine return types.

Feature-test macro
------------------

``-fherbceptions`` defines ``__HERBCEPTIONS__`` so code can detect the
feature:

.. code-block:: cpp

   #ifdef __HERBCEPTIONS__
   int foo() throws;
   #endif

Type traits
-----------

``-fherbceptions`` provides compiler builtins and the corresponding
``std`` traits for querying the type system:

.. code-block:: cpp

   // T can be thrown via `throw throws`: a usable error_domain<T> exists.
   static_assert(__is_herbception_throwsable(std::my_errc));

   // Function type is declared `fails{E}` (not plain `throws`).
   static_assert(__is_invoke_herbceptions_fails(decltype(f)));

   // { value_type, error_type } of an invoke-fails function type.
   using R = __invoke_herbception_fails_result<decltype(f)>;

Runtime support
===============

The ``libherbceptions`` runtime provides the ``error_domain_singleton``
vtables for the standard domains. Its API (``<herbceptions/error>``):

.. code-block:: cpp

   class error {
     // only the compiler fabricates std::error values; the type is not
     // copyable/constructible and runs the domain's do_cleanup on destruction
     [[nodiscard]] constexpr error_domain_singleton const *domain() const noexcept;
     [[nodiscard]] constexpr std::size_t code() const noexcept;
     template <class T> constexpr bool equivalent(T ec) const noexcept;
     constexpr std::errc to_errc() const noexcept;
     void throw_cxa_exception() const;   // rethrow as a traditional exception
     template <class T> constexpr bool is_code_of() const noexcept;
   };

   struct error_domain_singleton {
     void (*do_cleanup)(std::size_t) noexcept;
     bool (*do_equivalent)(std::size_t, error_domain_singleton const *, std::size_t) noexcept;
     void (*do_query_information)(std::size_t, error_query_information,
                                  error_reporter_encoding, void *,
                                  error_reporter_io_cookie_function) noexcept;
     std::errc (*do_to_errc)(std::size_t) noexcept;
     void (*do_throw_cxa_exception)(std::size_t, cxa_exception_abi) noexcept;
     void *__reserved[3];
   };

``do_query_information`` produces a domain *name* and/or *message* for an
error code, as a writev-style list of scatter pieces, encoded on request as
UTF-8/UTF-16/UTF-32. ``fast_io`` prints errors through it:

.. code-block:: cpp

   #include <fast_io.h>
   perrln(fast_io::mnp::name_message(e));   // e.g. [posix]Owner died

LLVM IR representation
======================

A function declared ``throws`` / ``fails{E}`` is lowered with the LLVM
``throws`` attribute and a struct return:

.. code-block:: llvm

   ; C++: T foo() throws;
   define { T, i1 } @foo(...) #throws { ... }

   ; C: void foo() fails{E};
   define { E, i1 } @foo(...) #throws { ... }

The ``i1`` discriminant is ``false`` for success and ``true`` for error. The
payload slot is a value-or-error union of size ``max(T, E)``: on success it
holds ``T``, on failure the error value. For the implicit ``std::error`` type
(a 2-register ``{void*, size_t}`` struct) a ``T throws`` function returns
``{ {ptr, i64}, i1 }``.

Call-site lowering
------------------

Callers of a ``throws`` function:

1. emit the call;
2. check the discriminant;
3. on success use the payload as the value;
4. on failure either propagate (return the error with the discriminant set)
   or branch to a handler (``catch fails`` / ``catch throws``).

Because a ``throws`` call is an ordinary call, there is no ``invoke``, no
landing pad, and no personality function on the pure-herbception path.
Cleanups run on the normal scope-exit path.

Legacy-EH interop (Itanium / MSVC / Wasm / SjLj)
------------------------------------------------

A ``throws`` function that can call ``noexcept(false)`` functions is wrapped
in a whole-function catch-all EH scope. Calls to ``noexcept(false)``
functions inside it become ``invoke``\ s into a landing pad whose handler
fabricates the ``std::error`` (via ``error_domain<std::cxa_exception_code>
::domain()`` and ``__cxa_get_exception_ptr`` / ``llvm.eh.exceptionpointer`` /
``wasm.get.exception``) and routes it to the throws return path. The same
conversion is applied inside a `try { } catch throws(std::error)` block.

A default ``fails{E}`` function instead pushes a terminate landing pad
(``noexcept`` semantics); an explicit ``fails{E} noexcept(false)`` pushes
nothing and lets traditional exceptions propagate.

Target-specific discriminant
============================

When the target reports ``supportThrowsCC()``, the backend can carry the
discriminant in a target-specific location instead of an extra register in
the struct return:

.. list-table::
   :header-rows: 1

   * - Target
     - Discriminant
     - Callee sets success / error
     - Caller checks
   * - x86-64
     - Carry flag (``CF`` in EFLAGS)
     - ``clc`` / ``stc``
     - ``setb`` / ``jc``
   * - AArch64
     - Carry flag (``C`` in NZCV)
     - ``subs xzr, xzr, xzr`` / ``subs xzr, xzr, #1``
     - ``cset w0, hs``
   * - ARM (32-bit)
     - Carry flag (``C`` in CPSR)
     - carry-flag set/clear
     - ``cset`` / conditional branch
   * - RISC-V
     - Extra register ``a2`` (X12)
     - ``li a2, 0`` / ``li a2, 1``
     - ``beqz a2, success``
   * - LoongArch
     - Extra register ``a2`` (R6)
     - ``li $a2, 0`` / ``li $a2, 1``
     - ``beqz $a2, success``

On targets without ``supportThrowsCC()``, the discriminant falls back to the
``{T, i1}`` struct return (two registers or sret).

Implementation notes
====================

The implementation spans the following areas:

* **Parsing** -- ``throws`` / ``fails{E}`` in
  ``clang/lib/Parse/ParseDeclCXX.cpp``; ``try(expr)``, ``catch fails(expr)``,
  ``throw throws expr``, ``failure(expr)`` in ``clang/lib/Parse/ParseExpr*.cpp``
  and ``ParseStmt.cpp`` (block handlers). ``try`` and ``catch`` are ``KEYHERB``
  keywords so they parse in C with ``-fherbceptions``.
* **Sema** -- ``ActOnHerbceptionTry``, ``ActOnHerbceptionCatchFails``,
  ``ActOnCXXThrowThrows``, ``ActOnHerbceptionFailure``,
  ``BuildCxaExceptionErrorValue`` in ``clang/lib/Sema/SemaExprCXX.cpp``;
  ``ActOnCXXCatchThrowsHandler`` in ``SemaStmt.cpp``; C enforces
  ``try()``/``catch fails()`` at call sites; C++ auto-propagates bare calls
  inside ``throws``/``fails`` functions (suppressed while parsing the
  operand of an explicit wrapper via ``Sema::HerbceptionOperandDepth``).
* **AST** -- ``CXXTryExpr``, ``CXXCatchFailsExpr`` and ``CXXCatchThrowsStmt``
  nodes; exception-spec storage for ``throws``/``fails{E}``; the N2289
  ``catch fails`` aggregate built by ``ASTContext::getCatchFailsType``.
* **CodeGen** -- ``{T, i1}`` return lowering with a payload sized
  ``max(T, E)``, the ``throws`` attribute, the whole-function legacy-EH
  conversion scope (``EmitStartEHSpec`` / ``getHerbceptionLegacyConvert`` in
  ``clang/lib/CodeGen/CGException.cpp``), ``catch throws``/``catch fails``
  routing, coroutine integration, and the carry-flag / extra-register
  discriminants in the backends.
* **Runtime** -- ``libherbceptions`` (``libherbceptions/{include,src}`):
  the ``error_domain_singleton`` vtables (posix, cxa_exception_code, win32,
  nt, com, wine), ``std::error``, and the
  name/message query protocol.

Known limitations
=================

* The ABI is experimental and not stable across compiler versions.
* The ``throws``/``fails`` specifier is *not* yet reflected in the mangled
  name, so a ``throws`` function and a plain function with the same
  signature currently share an Itanium mangling. (The return type *is*
  lowered, so this is safe only within one toolchain that agrees on the
  convention.)
* ``FastISel`` falls back to SelectionDAG for ``throws`` calls, so some
  ``-O0`` paths are slightly slower than they would otherwise be.
* Legacy C++ exception conversion requires the user's ``error_domain
  <std::cxa_exception_code>`` specialization (provided by ``libherbceptions``
  via ``<herbceptions/__details/cxa_exception_code.h>``); without it the
  conversion silently does not happen.
* The whole-function legacy conversion converts a ``throws`` function's
  escaped exceptions to ``std::error`` only; ``fails{E}`` keeps
  ``noexcept(true)`` semantics by default.

Related work
============

* Herb Sutter, `P0709R4: Zero-overhead deterministic exceptions
  <https://wg21.link/p0709r4>`_ -- the design this extension follows.
* Swift error handling and the LLVM ``swifterror`` attribute.
* Rust ``Result<T, E>`` and Go multi-value error returns.