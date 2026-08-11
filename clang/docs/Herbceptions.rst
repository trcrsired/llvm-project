.. _herbceptions:

================================================
Herbceptions: Zero-Overhead Deterministic Errors
================================================

.. contents::
   :local:

.. note::

   This is an **experimental** extension under active development. The
   implementation is incomplete, the ABI is not stable, and the design may
   change. Enable it with ``-fherbceptions``.

Overview
========

Herbceptions (after Herb Sutter's *P0709R4: Zero-overhead deterministic
exceptions*) replace the traditional two-phase exception model with a
**deterministic error channel**. Instead of throwing an object and unwinding
the stack through landing pads, a function that can fail marks itself with a
``throws`` (C++) or ``fails{E}`` (C and C++) specifier and returns the error
through the normal return path, discriminated by a boolean flag.

The two models are orthogonal:

* **Traditional EH** (``throw`` / ``try`` / ``catch``): requires
  ``-fexceptions`` and uses the unwinder, landing pads, and a personality
  function. The unwind is asynchronous and destructors run during phase-2
  unwinding.
* **Herbceptions** (``throw throws`` / ``try(expr)`` / ``catch fails(expr)``):
  enabled by ``-fherbceptions``. Errors are returned like ordinary values;
  there is no unwinder, no landing pad, no personality function, and no LSDA.
  Destructors run on the normal scope-exit path.

Herbceptions do not require ``-fexceptions``. The two flags are independent:
``-fherbceptions`` controls the deterministic error channel, while
``-fno-exceptions`` (or ``-fexceptions``) controls traditional EH.

Language-level design
=====================

Function specifiers
-------------------

Two specifiers exist and cannot coexist on the same function:

.. code-block:: cpp

   // C++ only. Implicit error type std::error.
   T foo() throws;

   // C++ and C. Explicit error type E.
   T foo() fails{E};

Semantically:

.. code-block:: cpp

   T foo() throws;        // noexcept(true) throws(true)
   T foo() fails{E};      // noexcept(true) fails{E}
   T foo() noexcept(false) fails{E};   // explicit override: allows both

``throws`` and ``fails{E}`` are part of the *canonical function type* and
change the function ABI (the return type is lowered to ``{T, i1}``). This is
unlike ``noexcept`` since C++17, which is not part of the canonical type.
Consequently:

* Function pointers with ``throws``/``fails`` are distinct types; there are
  **no implicit conversions** in either direction to/from plain function
  pointers.
* Redeclarations must agree on the exact specifier and (for ``fails{E}``) the
  exact error type.
* ``throws`` and ``fails{E}`` cannot be mixed in a redeclaration.

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

A ``fails{E}`` function uses the same syntax in the current implementation
(C++), or ``failure(expr)`` where that builtin exists (C):

.. code-block:: c

   int divide(int a, int b) fails{int} {
     if (b == 0) return failure(42);
     return a / b;
   }

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

The auto-propagation is suppressed while parsing the operand of an explicit
``try(expr)`` or ``catch fails(expr)``.

**C** -- calling a ``fails{E}`` function without an explicit wrapper is a
compile-time error. The error must be handled with ``try(expr)`` or
``catch fails(expr)``:

.. code-block:: c

   // error: calling function with 'fails{...}' specifier requires
   //        'try()' or 'catch fails()' wrapper
   int x = some_fails_func();

   int x = try(some_fails_func());                    // auto-propagate
   either(int,int) e = catch fails(some_fails_func()); // inspect

Explicit handling
-----------------

``try(expr)`` -- evaluates ``expr`` (a call to a ``throws``/``fails``
function); on failure it auto-propagates the error; on success it yields the
success value:

.. code-block:: cpp

   int foo(int x) throws {
     return try(bar(x)) + 1;   // if bar fails, foo fails with bar's error
   }

``catch fails(expr)`` -- evaluates ``expr`` and produces an ``either{T, E}``
value with fields ``.positive`` (bool), ``.left`` (success value of type
``T``) and ``.right`` (error value of type ``E``):

.. code-block:: cpp

   auto e = catch fails(bar(x));
   if (e.positive) {
     use(e.left);
   } else {
     handle(e.right);
   }

Catching errors with a block
----------------------------

``catch throws(E e) { ... }`` and ``catch fails(E e) { ... }`` provide
block-based handlers:

.. code-block:: cpp

   try {
     try foo();          // foo() throws
   } catch throws(std::error e) {
     if (e == std::errc::no_such_file_or_directory) { ... }
   }

``noexcept`` boundary
---------------------

A herbception error must never silently escape a ``noexcept(true)``
function. Calling a ``throws(true)`` function without handling it from a
``noexcept(true)`` function is a compile-time error, and ``try foo()`` (which
propagates) is likewise rejected there; use ``catch throws(...)`` instead.
``int main()`` is a special case: an unhandled error terminates via
``__builtin_trap()``.

Templates and concepts
----------------------

``try(expr)`` and ``catch fails(expr)`` accept dependent calls. Inside a
template the check is deferred to instantiation, and the herbception flag on
``throw throws`` is preserved when the expression is rebuilt, so instantiated
templates behave correctly. Constrained templates (concepts) work as usual.

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
payload slot holds the success value on success and the error value on
failure (a value-or-error union of size ``max(T, E)``).

Call-site lowering
------------------

Callers of a ``throws`` function:

1. emit the call;
2. check the discriminant;
3. on success use the payload as the value;
4. on failure either propagate (return the error with the discriminant set)
   or branch to a handler (``catch fails``).

Because a ``throws`` call is an ordinary call, there is no ``invoke``, no
landing pad, and no personality function. Cleanups run on the normal
scope-exit path.

Target-specific discriminant
============================

When the target reports ``supportThrowsCC()``, the backend can carry the
discriminant in a target-specific location instead of an extra register:

.. list-table::
   :header-rows: 1

   * - Target
     - Discriminant
     - Callee sets success / error
     - Caller checks
   * - x86-64
     - Carry flag (``CF`` in EFLAGS)
     - ``clc`` / ``stc`` (via ``add disc, -1``)
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
  ``throw throws expr`` in ``clang/lib/Parse/ParseExpr*.cpp``. ``try`` and
  ``catch`` are ``KEYHERB`` keywords so they parse in C with
  ``-fherbceptions``.
* **Sema** -- ``ActOnHerbceptionTry``, ``ActOnHerbceptionCatchFails``,
  ``ActOnCXXThrowThrows`` in ``clang/lib/Sema/SemaExprCXX.cpp``; C enforces
  ``try()``/``catch fails()`` at call sites; C++ auto-propagates bare calls
  inside ``throws``/``fails`` functions (suppressed while parsing the operand
  of an explicit wrapper via ``Sema::HerbceptionOperandDepth``).
* **AST** -- ``CXXTryExpr`` and ``CXXCatchFailsExpr`` nodes; exception-spec
  storage for ``throws``/``fails{E}``; the ``either{T, E}`` implicit record
  built by ``ASTContext::getEitherType``.
* **CodeGen** -- ``{T, i1}`` return lowering, the ``throws`` attribute,
  discriminant slot handling, coroutine integration, and the carry-flag /
  extra-register discriminants in the backends.
* **Mangling** -- ``throws``/``fails{E}`` are part of the canonical type, so
  they are reflected in the mangled name, preventing ABI mismatches at link
  time.

Known limitations
=================

* The ABI is experimental and not stable across compiler versions.
* ``fails{E}`` does not yet require the error type to be convertible to
  ``std::error`` when called from a ``throws`` function (the implicit
  ``std::error`` type is not yet wired into the front end; ``either{T, T}``
  is used as a placeholder when the callee has no explicit error type).
* ``failure(expr)`` is not yet a fully general builtin in C; the current
  implementation primarily supports ``throw throws`` / ``return failure(...)``
  where implemented.
* ``catch throws(E e) { }`` block handlers are partially implemented;
  ``try(expr)`` and ``catch fails(expr)`` are the fully working forms.
* Traditional exceptions cannot be converted to herbception errors
  (C++ exception -> ``std::error`` is not supported).
* FastISel falls back to SelectionDAG for ``throws`` calls, so some
  ``-O0`` paths are slightly slower than they would otherwise be.

Related work
============

* Herb Sutter, `P0709R4: Zero-overhead deterministic exceptions
  <https://wg21.link/p0709r4>`_ -- the design this extension follows.
* Swift error handling and the LLVM ``swifterror`` attribute.
* Rust ``Result<T, E>`` and Go multi-value error returns.
