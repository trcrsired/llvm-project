.. _herbceptions:

==========================================================
Herbceptions
==========================================================

.. contents::
   :local:

.. note::

   This is an **experimental** extension under active development. The
   implementation is incomplete, the ABI is not stable, and the design may
   change. Enable it with ``-fherbceptions``.

Overview
========

Herbceptions (after Herb Sutter's `P0709R4 <https://wg21.link/p0709r4>`_) replace the traditional two-phase
C++ exception model with a **deterministic error channel**. A function that
can fail declares *that it can fail* with a ``throws`` (C++) or ``return_failure{E}``
(C and C++) specifier, and the error is returned through the normal return
path, discriminated by a boolean flag carried next to the value.

The two models are orthogonal:

* **Traditional EH** (``throw`` / ``try`` / ``catch``): requires
  ``-fexceptions`` and uses the unwinder, landing pads, and a personality
  function. The unwind is asynchronous; destructors run during phase-2
  unwinding.
* **Herbceptions** (``throw throws`` / ``try(expr)`` / ``catch return_failure(expr)``):
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

Keyword naming
--------------

The original C standard proposal (N2289) used ``fails{E}`` and ``failure(expr)``
as the keyword names for the C-style error specifier and error-return
expression. These names were rejected in this implementation for two reasons:

* ``failure`` collides with ``std::ios_base::failure``, the standard exception
  class defined in ``<ios>``. Making ``failure`` a keyword breaks
  any code that includes ``<iostream>``, ``<ios>``, or any header that
  transitively defines ``std::ios_base::failure`` — a non-starter for a
  feature meant to interoperate with existing C++ code.
* Both ``fails`` and ``failure`` are common English words that appear as
  identifiers in real code (variable names, function names, member names).
  Keywords must be distinctive; reserving common words imposes an unacceptable
  migration burden on existing codebases.

This implementation uses ``return_failure`` instead. It serves both roles:

* ``return_failure{E}`` — function specifier (replaces ``fails{E}``).
* ``return_failure expr;`` — statement (replaces ``failure(expr)``). Parsed as
  a standalone statement like ``return`` or ``throw``; the operand is an
  expression and no parentheses are required. Also valid:
  ``return_failure(expr);``.

The keyword reads naturally in context: ``return_failure x;`` mirrors
``return x;`` and ``throw x;``. It is distinctive, does not collide with any
standard library identifier, and is unlikely to appear in existing code.

Language-level design
=====================

Function specifiers
-------------------

Two specifiers exist and cannot coexist on the same function:

.. code-block:: cpp

   // C++ only. Implicit error type std::error.
   T foo() throws;

   // C++ and C. Explicit error type E (trivially copyable).
   T foo() return_failure{E};

Semantically:

.. code-block:: cpp

   T foo() throws;                    // may fail via herbceptions channel; nounwind
   T foo() return_failure{E};         // may fail via herbceptions channel; nounwind

* ``throws`` and ``return_failure{E}`` are part of the *canonical function type* and
  change the function ABI (the return type is lowered to ``{T, i1}``). This
  is unlike ``noexcept`` since C++17, which is not part of the canonical
  type.
* ``throws(false)`` means the function cannot fail: it is equivalent to
  ``noexcept(true)``.
* ``throws``/``return_failure{E}`` and ``noexcept`` are mutually exclusive: a
  function picks one or the other, never both. ``throws`` supersedes ``noexcept``.
* In trait queries (``noexcept(expr)``, ``is_nothrow``), ``throws(true)`` / bare
  ``throws`` / ``return_failure{E}`` behave as ``noexcept(false)`` because the
  function may fail via the herbceptions channel. However, they are not
  semantically ``noexcept(false)`` — they use a different ABI and a different
  exception mechanism. In LLVM IR they are ``nounwind`` (no unwinding).
* ``return_failure(E)`` (parentheses) is rejected; the braces form is mandatory.
  Combining ``throws`` and ``return_failure{...}`` on one declaration is rejected.

Returning an error
------------------

A ``throws`` function returns an error with ``throw throws expr`` (C++
only); a ``return_failure{E}`` function returns an error with ``return_failure(expr)``
instead -- ``throw throws`` is not available inside ``return_failure{...}``
functions:

.. code-block:: cpp

   struct file {
     FILE *f;
     file(char const *path, char const *mode) throws : f(fopen(path, mode)) {
       if (!f)
         throw throws std::errc(errno);
     }
     ~file() { if (f) fclose(f); }
     file(const file &) = delete;
     file &operator=(const file &) = delete;
     FILE *get() const { return f; }
   };

   std::size_t read_file(char const *path, char *buf, std::size_t n) throws {
     file f{path, "rb"}; // a failing constructor auto-propagates
     std::size_t got = fread(buf, 1, n, f.get());
     if (ferror(f.get()))
       throw throws std::errc(errno); // f's destructor runs on the error edge
     return got;
   }

The operand is the error value; the compiler fabricates the (otherwise
unconstructible) ``std::error`` by evaluating
``std::error_domain<T>::domain()`` and ``std::error_domain<T>::code(e)``.
An operand whose type has no ``std::error_domain`` specialization is
rejected. Inside a ``catch throws`` handler the operand form is allowed
only when the enclosing function itself declares ``throws`` /
``return_failure{...}`` (the new error then leaves via its own channel); otherwise
use the bare rethrow instead (see below).

A ``return_failure{E}`` function returns an error with ``return_failure(expr)``
(C and C++), where ``expr`` has exactly the type ``E``:

.. code-block:: c

   int divide(int a, int b) return_failure{int} {
     if (b == 0)
       return_failure 42;
     return a / b;
   }

Bare ``throw throws`` (without an operand) rethrows the error currently
being handled and is only valid inside a ``catch throws`` handler.

Conditional ``throws``
``````````````````````

``throws`` accepts a constant-expression condition: ``throws(true)`` is
equivalent to a bare ``throws`` and ``throws(false)`` is equivalent to
``noexcept(true)`` -- the function then has no error channel at all. When
the condition is value-dependent the specifier stays dependent until
instantiation:

.. code-block:: cpp

   template <bool B>
   void maybe() throws(B);

   static_assert(throws(maybe<true>()));    // resolves to throws
   static_assert(noexcept(maybe<false>())); // resolves to noexcept

This enables the conditional-forwarding pattern, where a wrapper propagates
its callee's ability to fail exactly:

.. code-block:: cpp

   struct may_throw_int { static void go() throws; };
   struct no_throw_int  { static void go() noexcept; };

   template <typename S>
   void call_go() throws(!noexcept(S::go()));
   // can fail via herbceptions iff S::go() may throw a legacy exception

   static_assert(throws(call_go<may_throw_int>()));
   static_assert(!throws(call_go<no_throw_int>()));

``throws(expr)`` is also a unary operator, parallel to ``noexcept(expr)``:
it is true when evaluating ``expr`` may propagate an error through the
``throws`` channel. It only sees the ``throws`` channel -- a
``return_failure{E}`` callee does not report ``throws(expr)``:

.. code-block:: cpp

   void f() throws;
   int  g() return_failure{int};

   static_assert(throws(f()));   // throws channel: yes
   static_assert(!throws(g()));  // return_failure{E} does not use it

Restrictions
------------

* ``throws`` is only available in C++.
* ``return_failure{E}`` is a C-style feature restricted to **free functions**: member
  functions, lambdas, and coroutines cannot declare it.
* ``throws`` is **not allowed on coroutine declarations**; every herbception
  must be caught within the coroutine body.
* Destructors cannot be declared with a herbception specification.
* ``return_failure{std::error}`` is rejected: the implicit error type of ``throws``
  is compiler-fabricated and cannot be named explicitly.
* ``E`` must be trivially copyable.

Function pointers
`````````````````

Because the specifier changes the canonical type, function pointers with
``throws``/``return_failure`` are distinct types. There are **no implicit
conversions** in either direction to/from plain function pointers, and a
virtual override must use the same specifier as the base.

Error domains
`````````````

An error *type* must be registered with a ``std::error_domain``
specialization exposing at least a non-null ``domain()`` singleton pointer
and a ``code(E)`` projection. ``E`` need not be an enum -- any type with an
``error_domain<E>`` specialization can be thrown:

.. code-block:: cpp

   enum class my_errc : unsigned { ok = 0, bad = 1 };

   namespace std {
     template <> struct error_domain<my_errc> {
       static constexpr error_domain_singleton const *domain() noexcept;
       static constexpr unsigned long code(my_errc e) noexcept {
         return static_cast<unsigned long>(e);
       }
     };
   }

Returning ``nullptr`` from ``domain()`` is a compile-time error: the
fabricated ``std::error`` dereferences the domain pointer in its destructor.

Defining a domain
-----------------

``domain()`` returns the domain *singleton*: a unique, static-storage
``std::error_domain_singleton`` vtable that carries the domain's
operations. A complete minimal domain defines the vtable and the
specialization that references it:

.. code-block:: cpp

   #include <herbceptions/error>

   enum class my_errc : unsigned { ok = 0, bad = 1 };

   namespace {
   constinit std::error_domain_singleton my_domain{
     // Map a code onto std::errc for error::to_errc().
     .do_to_errc = [](std::size_t c) noexcept {
       return c == 1 ? std::errc::invalid_argument : std::errc{};
     },
   };
   }

   namespace std {
   template <> struct error_domain<my_errc> {
     static constexpr error_domain_singleton const *domain() noexcept {
       return &my_domain;
     }
     static constexpr std::size_t code(my_errc e) noexcept {
       return static_cast<std::size_t>(e);
     }
   };
   }

   int risky() throws { throw throws my_errc::bad; }

The ``error_domain_singleton`` vtable members:

* ``do_cleanup(code)`` -- run by ``~std::error()`` on the code; use it to
  release resources owned by the error value. May be ``nullptr``.
* ``do_equivalent(code, other_domain, other_code)`` -- semantic comparison
  behind ``error::equivalent``; lets different encodings mean the same
  error (e.g. ``ENOENT`` matching ``std::errc::no_such_file_or_directory``).
* ``do_query_information(code, query, encoding, cookie, emit)`` -- produce
  the domain *name* and/or error *message* as a writev-style scatter list
  (``io_scatter_t``) in the requested encoding, so generic printers can
  render the error without domain knowledge.
* ``do_to_errc(code)`` -- map the code onto ``std::errc``.
* ``do_throw_dynamic_exception(code, abi)`` -- rethrow the error as a
  traditional C++ exception; used by ``error::throw_dynamic_exception()``.

Two optional members of ``error_domain<E>`` refine interop:

* ``using domain_alias_type = error_domain<Other>;`` -- declares that
  ``Other``'s domain is an alias of this one; ``is_code_of<Other>``,
  ``equivalent`` and ``operator==`` then resolve through the aliased
  domain.
* ``static E from_std_error(std::error e)`` -- a projection back to ``E``;
  enables ``herbception_cast<E>(e)``.

The ``libherbceptions`` runtime ships ready-made domains (see `Predefined
domains`_), so user-defined domains are only needed for application- or
library-specific error types.

Calling a function that can fail
--------------------------------

**C++** -- a bare call to a ``throws``/``return_failure`` function inside a function
that itself has a ``throws``/``return_failure`` specifier **auto-propagates** the
error; no wrapper is required:

.. code-block:: cpp

   int process(int x) throws {
     int fd = open_wrapped(x); // auto-propagates on failure
     return fd;
   }

The auto-propagation is suppressed while parsing the operand of an explicit
``try(expr)`` or ``catch return_failure(expr)``.

**C** -- calling a ``return_failure{E}`` function without an explicit wrapper is a
compile-time error. The error must be handled with ``try(expr)`` or
``catch return_failure(expr)``:

.. code-block:: c

   // error: calling function with 'return_failure{...}' specifier requires
   //        'try()' or 'catch return_failure()' wrapper
   int x = some_return_failure_func();

   int x = try(some_return_failure_func());            // auto-propagate
   struct { union { int value; int error; }; bool failed; }
     e = catch return_failure(some_return_failure_func()); // inspect

Explicit handling
-----------------

``try(expr)`` -- evaluates ``expr`` (a call to a ``throws``/``return_failure``
function); on failure it auto-propagates the error; on success it yields the
success value. It is only valid inside a function that itself declares
``throws``/``return_failure{...}``:

.. code-block:: cpp

   int foo(int x) throws {
     return try(bar(x)) + 1; // if bar fails, foo fails with bar's error
   }

``catch return_failure(expr)`` -- evaluates ``expr`` and produces the N2289 aggregate
``struct { union { T value; E error; }; bool failed; }``. ``value`` and
``error`` are accessible through the anonymous union; ``failed`` is false on
success and true on error. It cannot be applied to a plain ``throws``
function (whose implicit ``std::error`` can only be handled by a
``catch throws`` block handler):

.. code-block:: cpp

   auto e = catch return_failure(bar(x));
   if (e.failed) {
     handle(e.error);
   } else {
     use(e.value);
   }

Catching errors with a block
----------------------------

``catch throws(std::error e) { ... }`` provides block-based handlers. The
handler must declare exactly ``std::error``, by value: references,
cv-qualified forms, other types and ``catch throws(...)`` are rejected, and
there is no block form of ``catch return_failure`` (it exists only as an expression).
A bare call to a ``throws``/``return_failure`` function inside the ``try`` block
routes the error value to the handler instead of propagating:

.. code-block:: cpp

   try {
     foo(); // foo() throws: the error is routed to the handler
   } catch throws(std::error e) {
     if (e == std::errc::no_such_file_or_directory) {
       ...
     }
   }

Inside the handler, bare ``throw throws`` rethrows the caught error.

One ``catch throws(std::error e)`` handler catches **both** herbception
errors and legacy C++ exceptions. When the ``try`` has no traditional
``catch`` clause, a legacy exception thrown inside it is auto-converted to
``std::error`` through the exception-pointer domain and delivered to the
same handler:

.. code-block:: cpp

   int open_db() throws;    // herbception error channel
   void legacy_init();      // noexcept(false): may throw e.g.
                            // std::ios_base::failure

   void run() {
     try {
       open_db();     // fails -> std::error on the herbception channel
       legacy_init(); // throws -> auto-converted to std::error
     } catch throws(std::error e) {
       // e is either open_db()'s std::error or the exception thrown by
       // legacy_init(), boxed as std::error.
       if (e.is_code_of<std::exception_ptr>())
         e.throw_dynamic_exception(); // rethrow the original C++ exception
     }
   }

See `Traditional exceptions`_ for how the conversion works and how the two
channels dispatch when traditional clauses are also present.

Additional rules for ``catch throws`` handlers:

* Herbception and traditional catch clauses may be mixed and interleaved in
  one try statement; the two channels dispatch independently. Legacy C++
  exceptions match only the traditional clauses (in their relative order,
  with ``catch(...)`` last among them); herbception errors scan only the
  ``catch throws`` handlers in their relative order.
* A ``throw throws`` inside a *traditional* handler chains to the next
  herbception handler after it.
* When a try has no traditional clauses, a ``catch throws(std::error)``
  handler also receives legacy exceptions auto-converted through the
  exception-pointer domain; with traditional clauses present they are
  delivered untouched instead.
* Inside a ``return_failure{E}`` function, a ``catch throws(std::error)`` handler
  requires a visible ``std::error_domain<E>`` specialization.
* Inside such a handler, a call to a plain ``return_failure{...}`` function must be
  wrapped in an explicit ``try()`` so its error is converted to
  ``std::error`` (C-style explicitness):

.. code-block:: cpp

   void g() return_failure{std::errc} {
     try {
       // ...
     } catch throws(std::error e) {
       auto r = try(return_failure_callee()); // ok: converted via error_domain
       // return_failure_callee();           // rejected: unconverted raw payload
     }
   }

Convertibility between specifiers
`````````````````````````````````

* Calling a ``return_failure{E}`` function from a ``throws`` function (by bare call
  or ``try(expr)``) converts the error to ``std::error`` through the
  ``std::error_domain<E>`` accessors (``domain()`` / ``code()``); a missing
  specialization is rejected.
* Calling a ``throws`` function from a ``return_failure{E}`` function propagates the
  fabricated two-word ``std::error`` payload verbatim into the error slot;
  no conversion is performed.

``noexcept`` boundary
---------------------

A herbception error must never silently escape a ``noexcept(true)``
function. Calling a ``throws`` function without handling it from a function
that is neither ``throws`` nor ``return_failure{...}`` is a diagnostic, and
``try(foo())`` (which propagates) is likewise rejected there; use a
``try { } catch throws(std::error e)`` block instead:

.. code-block:: cpp

   int compute() throws;

   void foo() {           // no spec
     compute();           // error: unhandled 'throws' call
     try(compute());      // error: 'try()' propagates, nowhere to go
     try {
       compute();         // ok: error is handled locally
     } catch throws(std::error e) { /* ... */ }
   }

   void bar() noexcept {  // noexcept does not help
     compute();           // error: ditto
   }

   void baz() throws {
     compute();           // ok: auto-propagates on baz's error channel
   }

``int main()`` is a special case: an unhandled error terminates via
``llvm.trap``.

``main`` itself cannot be declared ``throws`` or ``return_failure{...}``:
it is the outermost frame and has no caller able to consume a ``{T, i1}``
error result, so such a declaration is a compile-time error.

.. note::
   When an uncaught herbception traps in ``main()``, there is **no
   guarantee that any destructors run** before the trap. The trap is
   emitted directly on the error edge without unwinding ``main``'s cleanup
   stack, so destructors of ``main``'s local objects -- and the destructor
   of the escaping ``std::error`` value itself (which would run the
   domain's ``do_cleanup``) -- may all be skipped. This matches legacy C++
   exception semantics: an uncaught ``throw`` escaping ``main`` calls
   ``std::terminate``, and whether the stack is unwound (i.e. whether any
   destructors run) is implementation-defined there as well. Note that a
   hard stop is deliberate: ``abort()``/``__builtin_trap()`` never run
   *global* destructors either, and ``std::terminate()`` would bloat the
   fail-fast edge with the terminate-handler runtime -- a handler the
   committee may make overridable, and one whose identity is even left
   unspecified by [except.terminate] if a destructor run during unwinding
   replaced it -- pointless where global destruction isn't even possible.
   A direct crash is always the safest. Programs that rely on cleanup
   before termination should catch the error instead, e.g. with a
   function-try-block::

     int main()
     try {
       // ...
     } catch throws(std::error e) {
       // handle e, or terminate explicitly
     }

   On that path the error is routed into the handler's error slot, ``e``'s
   destructor runs when the handler exits, and ``main``'s local cleanups
   run normally.

Traditional exceptions
----------------------

A ``throws`` function implicitly **converts any legacy C++ exception that
escapes it** (thrown by a ``noexcept(false)`` callee) into a fabricated
``std::error`` on the herbception channel, exactly like a
``catch throws(std::error)`` handler does inside a ``try`` block. The
fabricated value pairs the exception-pointer domain singleton with the
caught-object pointer as its code, via the ``libherbceptions`` ABI entry
points ``__cxa_error_domain_{itanium,msvc}_exception_ptr()`` /
``__cxa_error_code_{itanium,msvc}_exception_ptr(ptr)``. Destructors still
run normally (during unwinding, since ``throws`` calls are plain
calls/invokes).

The conversion uses ``libherbceptions`` ABI entry points baked into the
compiler; the linker hard-errors if ``libherbceptions`` is not linked.

Example: a ``throws`` function that calls ``noexcept(false)`` legacy code
transparently converts any escaping exception to ``std::error`` on the
deterministic channel:

.. code-block:: cpp

   void legacy_io();              // noexcept(false); may throw
                                  // std::ios_base::failure

   int read_config() throws {     // implicit whole-function conversion
     legacy_io();                 // an escaping exception becomes a
     return 0;                    // std::error on the error channel
   }

   int caller() {
     try {
       return read_config();
     } catch throws(std::error e) { // receives herbception errors *and*
       report(e);                   // converted legacy exceptions
       return -1;
     }
   }

Inside a ``try`` block the same conversion applies to a
``catch throws(std::error e)`` handler when the try has no traditional
catch clause:

.. code-block:: cpp

   void parse_file() {
     try {
       legacy_parser();  // e.g. throws std::runtime_error
       fallible_step();  // a throws function
     } catch throws(std::error e) {
       // receives the herbception error of fallible_step() and the
       // std::runtime_error of legacy_parser(), converted to std::error
       // through the exception-pointer domain
     }
   }

When traditional clauses are also present the two channels dispatch
independently: legacy exceptions match only the traditional clauses and
herbception errors match only the ``catch throws`` clauses:

.. code-block:: cpp

   try {
     legacy_io();      // std::bad_alloc -> traditional clause
     fallible_step();  // std::error     -> herbception clause
   } catch (const std::exception &e) {   // legacy exceptions land here
     ...
   } catch throws(std::error e) {        // herbception errors land here
     ...
   }

In the other direction, ``std::error::throw_dynamic_exception()`` rethrows
the error as a traditional C++ exception through the domain's
``do_throw_dynamic_exception`` vtable entry.

A legacy ``throw`` that escapes a ``throws`` function -- whether it comes
from the function's own ``throw`` expression or from a ``noexcept(false)``
callee -- is delivered to callers as a ``std::error``, never as an unwind:

.. code-block:: cpp

   int compute() throws {
     // This legacy throw does not unwind out of compute(): the compiler
     // catches it at the function boundary and converts it to std::error.
     throw std::runtime_error("disk full");
   }

   void foo() {
     try {
       compute();
       throw std::runtime_error("boom"); // a direct legacy throw lands in
     } catch throws(std::error e) {      // the same handler
       // e arrived on the herbception channel both times: compute()'s error
       // and this runtime_error are boxed as std::error via the
       // exception-pointer domain.
       if (e.is_code_of<std::exception_ptr>())
         e.throw_dynamic_exception(); // recover the original exception
     }
   }

Templates and concepts
----------------------

``try(expr)`` and ``catch return_failure(expr)`` accept dependent calls. Inside a
template the check is deferred to instantiation, and the herbception flag on
``throw throws`` is preserved when the expression is rebuilt, so
instantiated templates behave correctly. Constrained templates (concepts)
work as usual.

Constexpr
---------

``return_failure{E}`` functions, ``throw throws``, ``try(expr)`` auto-propagation,
``catch return_failure(expr)`` and ``try { } catch throws(std::error)`` blocks are
usable in constant expressions:

.. code-block:: cpp

   constexpr int f(int x) return_failure{int} {
     if (x == 0)
       return_failure(42);
     return 2 * x;
   }
   constexpr int g(int x) return_failure{int} {
     return try(f(x));
   }
   static_assert(g(3) == 6);
   static_assert(g(0) == 42);

At compile time the fabricated domain pointer is a unique opaque constant,
so ``e.code()`` and ``e == errc_value`` comparisons work in constant
expressions.

A call to a ``throws``/``return_failure{E}`` function inside a
constant-expression context (a ``constexpr`` variable initializer,
``static_assert``, a constant template argument, ...) is **not** rejected
merely because the lexically enclosing function lacks a
``throws``/``return_failure{...}`` spec. The call is evaluated at compile
time, like ``operator new`` in a constant expression: the operation is
permitted inside the evaluation, but its effects must not escape it.
A herbception error that is thrown *and caught* within the evaluation is
fine; an error that reaches the boundary of the constant expression
uncaught makes it not a constant expression -- you cannot throw out of
constant evaluation:

.. code-block:: cpp

   constexpr int parse(const char *s) throws;

   constexpr int safe(const char *s) {
     try { return parse(s); }
     catch throws(std::error e) { return -1; } // caught inside: OK
   }

   constexpr int a{parse("42")};  // OK: evaluated at compile time, no error
   constexpr int b{safe("abc")};  // OK: the error is caught by the handler
                                  // inside the evaluation
   constexpr int c{parse("abc")}; // error: a 'throws' error escapes the
                                  // constant expression

Coroutines
----------

Coroutines cannot declare ``throws`` or ``return_failure{...}``; all herbceptions
must be caught within the coroutine body.

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
``std`` traits (in ``<herbceptions/error>``) for querying the type system:

.. code-block:: cpp

   // T can be thrown via `throw throws`: a usable error_domain<T> exists.
   static_assert(__is_herbceptions_throwsable(my_errc));

   // Function type is declared `return_failure{E}` (not plain `throws`).
   static_assert(__is_invoke_herbceptions_return_failure(decltype(f)));

   // { value_type, error_type } of an invoke-return_failure function type.
   using R = __invoke_herbceptions_return_failure_result<decltype(f)>;

Additional traits mirror the classic ``traits`` family along the herbception
channel: ``__is_herbceptions_throws_constructible``,
``__is_herbceptions_throws_assignable``,
``__is_herbceptions_throws_convertible``,
``__has_herbceptions_throws_constructor``, ``__has_herbceptions_throws_copy``,
``__has_herbceptions_throws_assign`` and
``__has_herbceptions_throws_move_assign``. The type trait
``__invoke_herbceptions_return_failure_t`` yields the raw ``{T, i1}``-shaped result
type of a ``return_failure`` function type.

Convenience specializations of ``__is_herbceptions_throws_constructible`` mirror
the standard ``is_nothrow_copy_constructible`` / ``is_nothrow_move_constructible``
family::

   // Copy/move construction that can propagate a herbception.
   static_assert(std::is_herbceptions_throws_copy_constructible_v<T>);
   static_assert(std::is_herbceptions_throws_move_constructible_v<T>);

Invocation traits test whether a callable can propagate a herbception error
through the ``throws`` channel when invoked::

   // F is invocable with Args... and the call can throw.
   static_assert(std::is_herbceptions_throws_invocable_v<F, Args...>);
   // Same, and the result is convertible to R.
   static_assert(std::is_herbceptions_throws_invocable_r_v<R, F, Args...>);

Runtime support
===============

The ``libherbceptions`` runtime (header ``<herbceptions/error>``) provides
the ``std::error`` class, the ``std::error_domain<T>`` customization point,
the ``error_domain_singleton`` vtables for the standard domains, and the
trait aliases described in `Type traits`_.

``std::error``
--------------

``std::error`` is a two-word ``{domain, code}`` value that only the
compiler can fabricate: default, copy and move construction and assignment
are all deleted, so it cannot be created, copied or stored -- it exists
transiently on the error channel and inside ``catch throws`` handlers.
``~error()`` runs the domain's ``do_cleanup`` on the code.

.. code-block:: cpp

   class error {
     // all constructors and assignment deleted; constexpr ~error() runs
     // the domain's do_cleanup
     [[nodiscard]] constexpr error_domain_singleton const *domain() const noexcept;
     [[nodiscard]] constexpr std::size_t code() const noexcept;
     template <class T> constexpr bool equivalent(T ec) const noexcept;
     constexpr std::errc to_errc() const noexcept;
     void throw_dynamic_exception() const; // rethrow as a C++ exception
     template <class T> constexpr bool is_code_of() const noexcept;
   };

* ``domain()`` -- the ``error_domain_singleton`` vtable of the domain the
  error came from; never null.
* ``code()`` -- the raw code value minted by ``error_domain<E>::code(e)``.
* ``e == v`` -- *exact* match: ``v``'s domain (or its ``domain_alias_type``)
  is the error's domain and the codes are equal. ``v`` is any type with an
  ``error_domain`` specialization.
* ``e.equivalent(v)`` -- *semantic* match, routed through
  ``do_equivalent``, so errors in different domains that mean the same
  thing compare equal (e.g. ``ERROR_FILE_NOT_FOUND`` equivalent to
  ``std::errc::no_such_file_or_directory``).
* ``e.to_errc()`` -- map the code to ``std::errc`` via ``do_to_errc``.
* ``e.is_code_of<T>()`` -- whether the error belongs to ``T``'s domain
  (honoring ``domain_alias_type``).
* ``e.throw_dynamic_exception()`` -- rethrow the error as a traditional
  C++ exception via ``do_throw_dynamic_exception``; falls back to
  ``std::system_error(e.to_errc())``. Deleted when exceptions are disabled.
* ``herbception_cast<E>(e)`` -- convert the error back to ``E`` when
  ``error_domain<E>`` provides ``from_std_error``.

.. code-block:: cpp

   try {
     fallible_step();
   } catch throws(std::error e) {
     if (e == std::errc::no_such_file_or_directory) { /* exact match */ }
     else if (e.equivalent(std::errc::timed_out))   { /* semantic match */ }
     else if (e.is_code_of<std::exception_ptr>())
       e.throw_dynamic_exception();  // recover the original exception
     report(e.to_errc());
   }

Predefined domains
------------------

``libherbceptions`` ships ready-made domains, each exposed through an
extern ``"C"`` factory declared in namespace ``std::error_domains`` of
``<herbceptions/error>``:

.. list-table::
   :header-rows: 1

   * - Factory
     - Error type ``E``
     - Covers
   * - ``__cxa_error_domain_posix()``
     - ``std::errc``
     - ``errno`` / POSIX error codes
   * - ``__cxa_error_domain_win32()``
     - ``std::win32_errc``
     - Win32 ``GetLastError`` codes
   * - ``__cxa_error_domain_nt()``
     - ``std::nt_errc``
     - ``NTSTATUS`` values
   * - ``__cxa_error_domain_com()``
     - ``std::com_errc``
     - COM ``HRESULT`` values
   * - ``__cxa_error_domain_wine()``
     - ``std::wine_errc``
     - Wine Unix ``errno`` codes
   * - ``__cxa_error_domain_cmath()``
     - ``std::cmath_errc``
     - ``<fenv.h>`` floating-point flags
   * - ``__cxa_error_domain_parse()``
     - ``std::parse_errc``
     - parser errors (eof / partial / invalid / overflow)
   * - ``__cxa_error_domain_{itanium,msvc}_exception_ptr()``
     - ``std::exception_ptr``
     - legacy C++ exceptions (see `Traditional exceptions`_)

Each ``__details/<name>.h`` header declares the corresponding
``std::error_domain<E>`` specialization, so a ``throw throws std::errc(e)``
or ``throw throws std::nt_errc(s)`` works out of the box once the runtime
is linked.

ABI and mangling
================

A ``throws`` / ``return_failure{E}`` function is lowered with the LLVM
``throws`` attribute and a ``{T, i1}`` struct return: the first element is a
``max(T, E)``-sized value-or-error union and the trailing ``i1`` is the
discriminant -- ``false`` on success, ``true`` on error. There is no
``invoke``, landing pad, personality function or LSDA on the
pure-herbception path: a ``throws`` call is an ordinary call, and the
caller simply checks the discriminant. On targets that support it the
backend carries the discriminant in a dedicated location -- the carry flag
on x86/AArch64/ARM/SPARC/ARM64EC, an extra register on
RISC-V/LoongArch/MIPS/Xtensa, the ``cr6`` field on PowerPC, or an extra
multivalue result on WebAssembly -- so checking it costs a single
conditional branch. When a payload does not fit the register-return budget
it is constructed through a hidden ``throws_sret`` buffer and the
registers carry only ``{E, i1}``.

The specifier is part of the function's mangled name (``Dr`` /
``DE <type> E`` / ``Dg <expr> E`` in the Itanium ABI, ``_H`` /
``_F <type>`` in the MSVC ABI), so a ``throws`` function and a plain
function with the same signature get different symbols. Translation units
that disagree about a specifier fail to link with an unresolved-symbol
error rather than silently mis-calling. Under LTO (``-flto``, full or
thin), ``ld.lld`` additionally compares the herbception signature of every
externally visible bitcode function and reports an ODR violation on a
conflict:

.. code-block:: none

   ld.lld: error: herbception ODR violation: symbol '_Z3fooi' has
   conflicting definitions: it is defined as a herbception ('throws')
   function with error payload type '{ ptr, i64 }' in 'a.o', but without
   the herbception error channel in 'b.o'

For the full lowering rules, the per-target discriminant conventions, the
legacy-EH interop mechanism and the mangling encodings, see
:ref:`herbceptions-implementation`.


Known limitations
=================

* The ABI is experimental and not stable across compiler versions.
* ``FastISel`` falls back to SelectionDAG for ``throws`` calls, so some
  ``-O0`` paths are slightly slower than they would otherwise be.
* Legacy C++ exception conversion depends on the ``libherbceptions``
  runtime ABI symbols baked into the compiler; the linker hard-errors if
  ``libherbceptions`` is not linked.

Related work
============

* Herb Sutter, `P0709R4: Zero-overhead deterministic exceptions
  <https://wg21.link/p0709r4>`_ -- the design this extension follows.
* Swift error handling and the LLVM ``swifterror`` attribute.
* Rust ``Result<T, E>`` and Go multi-value error returns.
