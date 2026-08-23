.. _herbceptions-implementation:

Herbceptions Implementation Notes
=================================

.. contents::
   :local:

This document describes the actual implementation of the herbception
``throws`` / ``fails{E}`` extension in Clang/LLVM and the accompanying
``libherbceptions`` runtime. It is written for Clang/LLVM developers; for
the user-facing language description and motivation, see
:ref:`herbceptions`.

Everything below reflects the current state of the implementation on the
``herbception-analysis`` branch.

Overview
========

Herbceptions are a deterministic error channel layered on the normal return
path. A function declared ``throws`` (implicit ``std::error``) or
``fails{E}`` (explicit error type) is lowered so that:

* the IR return type becomes ``{ T, i1 }`` (payload + discriminant);
* the LLVM function carries the ``throws`` attribute;
* on success the payload holds ``T`` and the discriminant is ``false``;
* on failure the payload holds the error value and the discriminant is
  ``true``.

There is no unwinder, landing pad, or personality function on the
pure-herbception path. Traditional C++ exceptions are supported only
through explicit interop points (see `Legacy C++ EH interop`_).

Language surface
================

Function specifiers
-------------------

``throws`` is a C++-only specifier with the implicit error type
``std::error``. ``fails{E}`` works in C and C++ and carries the explicit
error type ``E``. Both are stored in the function type's exception
specification:

* ``EST_BasicThrows`` -- bare ``throws`` (``clang/include/clang/AST/
  TypeBase.h``, see ``FunctionProtoType::hasBasicThrowsSpec()``).
* ``EST_ThrowsTyped`` -- ``fails{E}``; ``E`` is stored in the exception-type
  slot.
* ``EST_ThrowsTypedNoexceptFalse`` -- ``fails{E} noexcept(false)``.

A ``throws`` function implies ``noexcept(true)``; combining with
``noexcept(false)`` is rejected (``SemaDeclAttr.cpp`` /
``Sema::checkExceptionSpecification``). ``throws(false)`` degrades to plain
``noexcept``. ``fails{E}`` implies ``noexcept(true)`` by default and is
trivially-copyable-checked.

The specifiers are part of the canonical function type because they change
the calling convention (``{T, i1}`` return). Function pointers, overload
resolution and virtual overrides therefore treat them as distinct.

Expressions and statements
--------------------------

* ``throw throws`` -- ``ActOnCXXThrowThrows``
  (``clang/lib/Sema/SemaExprCXX.cpp``). Bare ``throw throws`` (no operand)
  rethrows the caught error inside a ``catch throws`` handler. ``throw throws
  expr`` with an explicit operand is disallowed.
* ``try(expr)`` -- ``CXXTryExpr``. Evaluates a ``throws``/``fails`` call;
  auto-propagates the error.
* ``catch fails(expr)`` -- ``CXXCatchFailsExpr``. Produces the N2289
  aggregate ``struct { union { T value; E error; }; bool failed; }`` built by
  ``ASTContext::getCatchFailsType``.
* ``failure(expr)`` -- C/C++ way to return an error from a ``fails{E}``
  function (``ActOnHerbceptionFailure``).
* ``try { ... } catch throws(E e) { }`` / ``catch fails(E e) { }`` -- block
  handlers parsed by ``ParseStmt.cpp`` and checked by
  ``Sema::ActOnCXXCatchThrowsHandler`` (``CXXCatchThrowsStmt``).

Auto-propagation
----------------

In C++, a bare call to a ``throws``/``fails`` function inside a
``throws``/``fails`` function auto-propagates the error:
``Sema::HerbceptionOperandDepth`` suppresses the auto-propagation while
parsing the operand of ``try(expr)`` / ``catch fails(expr)``. In C, calling a
``fails{E}`` function without a wrapper is a compile-time error
(``err_herbception_c_bare_call``).

``noexcept`` boundary
---------------------

Calling a ``throws`` function bare (or with ``try``) from a ``noexcept(true)``
function is rejected (``err_herbception_noexcept_calls_throws`` in
``clang/include/clang/Basic/DiagnosticSemaKinds.td``). ``main()`` is a
special case: an unhandled error traps via ``__builtin_trap()``
(``clang/lib/CodeGen/CGCall.cpp``, ``herb.main.trap`` block).

Type traits
-----------

``SemaTypeTraits.cpp`` implements ``__is_herbception_throwsable``,
``__is_invoke_herbceptions_fails`` and ``__invoke_herbception_fails_result``,
mirrored as ``std::is_herbception_throwsable`` etc. in
``libherbceptions/include/herbceptions/error``.

AST
===

Nodes
-----

* ``CXXTryExpr`` -- ``try(expr)``.
* ``CXXCatchFailsExpr`` -- ``catch fails(expr)``.
* ``CXXCatchThrowsStmt`` -- the ``catch throws(E e)`` / ``catch fails(E e)``
  handler. It stores the optional *legacy conversion expression*
  (``getLegacyExceptionErrorValue()``): the ``std::error`` fabrication for a
  caught traditional C++ exception. Built by
  ``Sema::ActOnCXXCatchThrowsHandler`` only when the handler binds
  ``std::error`` and the ``error_domain<std::cxa_exception_code>``
  specialization is visible.
* ``CXXErrorValueExpr`` -- a compiler-fabricated ``{domain, code}`` value that
  users cannot construct.
* ``CXXCxaExceptionExpr`` -- the thrown-object pointer used as the ``code``
  of a converted legacy exception.

Exception specification storage
-------------------------------

``FunctionProtoType`` stores the herbception specifier in
``FunctionTypeBits.ExceptionSpecType`` (see ``TypeBase.h``) and, for
``fails{E}``, the error type in the exceptions slot. The specifier is part of
the canonical type (``getCanonicalExceptionSpecType``).

Sema
====

``SemaExprCXX.cpp``
-------------------

* ``ActOnHerbceptionTry`` -- builds ``CXXTryExpr``; validates the operand is a
  ``throws``/``fails`` call (deferred inside templates).
* ``ActOnHerbceptionCatchFails`` -- builds ``CXXCatchFailsExpr``; rejects a
  ``throws`` operand (no explicit error type to name).
* ``ActOnCXXThrowThrows`` -- bare ``throw throws`` (rethrow in a ``catch
  throws`` handler); ``throw throws expr`` with an operand is disallowed.
* ``ActOnHerbceptionFailure`` -- ``failure(expr)``; only valid inside a
  ``fails{E}`` function, operand must be of type ``E``.
* ``BuildCxaExceptionErrorValue`` -- best-effort fabrication of the
  ``std::error`` for a legacy exception:
  ``{ error_domain<std::cxa_exception_code>::domain(),
  __cxa_get_exception_ptr(...) }``. Returns ``ExprError`` silently when the
  ``cxa_exception_code`` type / domain / ``std::error`` are not visible.
* The ``error_domain<T>::domain()`` null check for the
  ``std::error_domain`` definition is in ``ActOnFinishFunctionBody``
  (``SemaDecl.cpp``): returning ``nullptr``/``0`` from ``domain()`` is
  diagnosed (the fabricated ``std::error`` dereferences it in ``~error()``).

``SemaStmt.cpp``
----------------

* ``ActOnCXXCatchThrowsHandler`` -- builds ``CXXCatchThrowsStmt``; attaches
  the legacy conversion expression for a ``std::error`` handler.
* ``ActOnCXXTryStmt`` -- recognizes a try block containing herbception
  handlers (``CXXCatchThrowsStmt``) and marks it for the CodeGen path that
  routes the discriminant.

Whole-function conversion
-------------------------

A bare ``throws`` function implicitly converts any legacy C++ exception that
escapes it. ``Sema::ActOnFinishFunctionBody`` (``SemaDecl.cpp``) builds
``BuildCxaExceptionErrorValue`` for a ``EST_BasicThrows`` definition and
stores it on the ``FunctionDecl`` via
``setHerbceptionLegacyErrorValue()``. The member is serialized by
``ASTWriterDecl.cpp`` / ``ASTReaderDecl.cpp``.

CodeGen
=======

``{T, i1}`` lowering
--------------------

``clang/lib/CodeGen/CGCall.cpp``: ``FunctionProtoType::hasThrowsSpec()`` is
threaded through ``CGFunctionInfo`` (``hasThrowsReturn()``). The return ABI
is a ``{ T, i1 }`` struct; the payload slot is sized to
``max(sizeof(T), sizeof(E))`` via ``getHerbceptionErrorType``
(``std::error`` is a 2-register ``{void*, size_t}`` struct). The ``throws``
attribute is added to the function and call site
(``llvm::Attribute::Throws``).

Call-site routing
`````````````````

``clang/lib/CodeGen/CGCall.cpp`` ``EmitCall``: after the call, the
discriminant is extracted. Depending on the context:

* an enclosing ``catch throws`` / ``catch fails`` scope
  (``CodeGenFunction::HerbceptionCatchScopes``): store the error value into
  the handler's slot and branch to the handler;
* a ``throws``/``fails`` function: store the error value, set the
  discriminant, run cleanups, branch to the return block
  (``EmitHerbceptionThrow`` in ``CGStmt.cpp``);
* ``main()``: trap on error;
* otherwise: ``err_herbception_noexcept_calls_throws``.

The ``{T, i1}`` heuristic guards against misclassifying a genuine two-field
struct return: only a second struct element of type ``i1`` is treated as a
throws discriminant (``CGCall.cpp``).

``try(expr)`` / ``catch fails(expr)``
-------------------------------------

``CodeGenFunction::EmitHerbceptionTry`` (``CGStmt.cpp``) emits the call,
reads the discriminant, auto-propagates on error (running cleanups).
``EmitHerbceptionCatchFails`` emits the N2289 aggregate: stores ``value`` and
sets ``failed=false`` on success, ``error`` and ``failed=true`` on failure.
``EmitFailsErrorToStdError`` converts a ``fails{E}`` error to ``std::error``
when calling into a ``throws`` function.

Block handlers
``````````````

``clang/lib/CodeGen/CGException.cpp`` ``EmitCXXTryStmt`` routes try
statements that contain ``CXXCatchThrowsStmt`` handlers to
``EmitHerbceptionCatchTry``. It creates one block and error slot per handler,
pushes them on ``HerbceptionCatchScopes`` (so bare calls inside the try body
route to them), and emits the dispatch. Cleanups (including the caught
variable's destructor, which runs the domain's ``do_cleanup``) execute
exactly once.

Coroutines
----------

``clang/lib/CodeGen/Coroutines.cpp`` and ``CGCall.cpp``: a ``throws``
coroutine's ramp returns ``{Task, i1}`` with the ``throws`` attribute. The
discriminant is kept on the stack (``!coro.outside.frame`` metadata) because
the coroutine frame may be deallocated before the ramp reads it back.

Legacy C++ EH interop
=====================

``catch throws(std::error e)`` inside a try block
-------------------------------------------------

When a handler binds ``std::error``, ``EmitHerbceptionCatchTry`` pushes a
catch-all EH scope around the try block, so calls to ``noexcept(false)``
functions inside become ``invoke``\ s into a landing pad. The handler block
(``herb.legacy.convert``) fabricates the ``std::error``
(``EmitErrorValueExpr`` + ``EmitCxaExceptionPtr`` in ``CGStmt.cpp``) and
routes it to the handler. Personality-dependent:

* Itanium / SjLj: ``__cxa_get_exception_ptr(exn.slot)``.
* MSVC: ``llvm.eh.exceptionpointer`` from the funclet ``catchpad``.
* Wasm: ``wasm.get.exception`` (object already stored in ``exn.slot``).

Whole-function conversion
-------------------------

For a bare ``throws`` function with a stored conversion expression,
``EmitStartEHSpec`` (``CGException.cpp``) pushes a whole-function catch-all
EH scope whose handler is ``getHerbceptionLegacyConvert()``. The handler
fabricates the ``std::error`` and calls ``EmitHerbceptionThrow``, routing it
to the throws return path. ``EmitEndEHSpec`` pops the scope;
``FinishFunction`` emits the block if used. ``EmitStartEHSpec``/``EmitEndEHSpec``
also handle the ``fails{E}`` terminate scope:

* default ``fails{E}`` (implies ``noexcept(true)``) -> terminate landing pad;
* ``fails{E} noexcept(false)`` -> nothing (traditional exceptions propagate).

Backend
=======

``supportThrowsCC()`` (``llvm/include/llvm/CodeGen/TargetLowering.h``)
advertises target-specific discriminant carrying; overridden by X86,
AArch64, ARM, RISC-V, LoongArch and WebAssembly. See :ref:`herbceptions` for
the mechanism per target. When false, the discriminant is a regular struct
member.

Win64 expanded ABI
------------------

The standard Win64 ABI returns scalars only in RAX and passes types larger
than 8 bytes by pointer.  This conflicts with herbceptions because the
discriminant lives in the carry flag (CF), requiring the payload to reside
in registers.

The following changes apply only to functions with the ``throws``
attribute; non-``throws`` functions follow the standard Win64 ABI
unchanged.

**Empty structs.** An empty struct (zero-sized) does not consume a
register slot for the return value.  ``GetReturnInfo`` skips zero-sized
types when the ``throws`` attribute is present
(``llvm/lib/CodeGen/TargetLoweringBase.cpp``).

**Return values (RAX+RDX).** ``CanLowerReturn`` returns ``true`` for
Win64 ``throws`` functions when every non-discriminant return part is
at most i64 after decomposition.  The standard calling convention
(``RetCC_X86``) already assigns i64 leaves to ``[RAX, RDX, RCX, R8]``
via ``RetCC_X86Common``, so the two i64 halves of a 16-byte payload
naturally land in RAX and RDX.  The i1 discriminant is carried in CF
via the ADD-with-AllOnes trick and never consumes a register.

**Parameters (RCX+RDX / R8+R9 / stack).**  The ``WinX86_64ABIInfo``
frontend ABI classifier (``clang/lib/CodeGen/Targets/X86.cpp``) is
extended with an ``IsThrows`` flag threaded through ``computeInfo``.
When the function carries a herbception error type, record types that
are exactly 16 bytes and have no destructor
(``isDestructedType() == DK_none``) are coerced to ``{i64, i64}``
instead of being passed by pointer.  ``ComputeValueTypes`` decomposes
the coerced struct into two i64 leaves; each leaf independently
consumes one of the four integer parameter registers (RCX, RDX, R8,
R9), or spills to the stack.  This matches the i686 Windows fastcall
pattern where two i32 values split into ECX+EDX.

Types with a destructor, types not exactly 16 bytes, and non-power-of-two
sizes continue to follow the standard Win64 rule (passed by pointer).

**Frame pointer.** Win64 ``throws`` functions force a frame pointer
(``X86ISelLoweringCall.cpp``) so the epilogue uses ``MOV RSP, RBP``
(which does not touch EFLAGS) instead of ``ADD RSP, imm`` (which
clobbers CF before the ``ret``).

**New calling convention definitions.** ``RetCC_X86_Win64_C_Throws``
and ``CC_X86_Win64_C_Throws`` are documented in
``llvm/lib/Target/X86/X86CallingConv.td``.  They mirror the standard
Win64 conventions but serve as explicit, named entry points for the
expanded register set.

Runtime: libherbceptions
========================

``libherbceptions/`` in the monorepo:

* ``include/herbceptions/error`` -- ``std::error`` (compiler-only
  fabrication; non-copyable, runs ``do_cleanup`` in ``~error()``),
  ``error_domain_singleton`` (``do_cleanup``, ``do_equivalent``,
  ``do_query_information``, ``do_to_errc``, ``do_throw_cxa_exception``,
  ``__reserved[3]``), and the type traits.
* ``src/{posix,cxa_exception,win32,nt,com,wine}.cpp`` -- one translation
  unit per domain, each providing a weak ``__cxa_error_domain_*`` ABI entry
  point. ``posix`` and ``cxa_exception_code`` are always built; the Windows
  domains only on ``_WIN32``/``__CYGWIN__``.
* ``src/domain_helpers.h`` -- ``query_information_pieces``, a writev-style
  collector used by ``do_query_information``: add up to 8 ``io_scatter_t``
  pieces, then ``emit(encoding, cookie, cookfun)``. Codecvt happens in
  ``emit`` (UTF-8 -> UTF-16/UTF-32, byte encodings pass through).
* ``src/ntkernel.h`` / ``ntkernel-table.ipp`` -- the NTSTATUS table with
  message lengths and binary-search ``find_ntstatus``.
* ``test/`` -- ``domain_test.cpp`` (domain identity, ``do_to_errc``,
  cross-domain equivalence, ``do_query_information`` output).

Testing in Clang/LLVM
=====================

Compiler behavior is covered by ``clang/test/CodeGen/herbception-*.cpp`` and
``clang/test/Sema/herbception-*.cpp`` (``-fherbceptions``):
``herbception-throws.cpp`` (``{T, i1}`` lowering, ``try(expr)``),
``herbception-autoprop.cpp`` (auto-propagation, ``catch fails``),
``herbception-catch-throws.cpp`` (block handler), ``herbception-catch-fails.cpp``,
``herbception-legacy-convert.cpp`` (the ``catch throws(std::error)`` legacy
conversion on Itanium/MSVC/Wasm/SjLj), ``herbception-fails-noexcept.cpp``
(terminate vs. propagate), ``herbception-coroutine.cpp``,
``herbception-two-field-struct.cpp`` (the ``{T, i1}`` heuristic),
``herbception-constexpr.cpp``, ``herbception-fnptr.cpp``,
``herbception-throws-noexcept.cpp``, ``herbception-dtor-spec.cpp``,
``herbception-fails-trivially-copyable.cpp``, ``herbception-domain-nullptr.cpp``,
``herbception-traits.cpp`` and ``herbception-c-bare-call.c``.

Known limitations
=================

* The ``throws``/``fails`` specifier is not yet reflected in the mangled
  name.
* ``FastISel`` falls back to SelectionDAG for ``throws`` calls.
* Legacy-EH conversion is best-effort and depends on the user's
  ``error_domain<std::cxa_exception_code>`` specialization.
