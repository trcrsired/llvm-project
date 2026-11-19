.. _safecxx:

=========================================================
Safe C++
=========================================================

.. contents::
   :local:

.. note::

   This is an **experimental** extension under active development. The design
   is still being settled and the implementation is incomplete. The authoritative
   design record lives in the ``safecppdesign`` repository's ``DESIGN.md``; this
   page is the Clang user guide for the pieces that are implemented here.

Overview
========

Safe C++ is a **purely additive dialect**: it changes no existing C++ semantics.
Everything that is unsafe remains available, but only inside an explicitly
written ``[[unsafe]] { }`` fence. Code that opts out compiles as ordinary C++
with the same behavior and the same code generation. The governing principle is
"do not interfere with any existing C++ semantics" — the mistake a from-scratch
safe language would make.

The dialect is composed of five parts:

* **Herbceptions** (:ref:`herbceptions`) — a deterministic ``{T, i1}`` error
  channel instead of the unwinder.
* **P1179-style lifetime analysis** — a small, per-value provenance protocol
  expressed with ``[[owner]]`` and ``[[lifetime(this)]]``, scoped to the
  ``in`` / ``out`` / ``inout`` / ``move`` parameter kinds.
* **d0708 parameter passing** — ``in``, ``out``, ``inout``, ``move`` and
  ``destructive_move`` contextual keywords that replace the
  ``const T&`` / ``T&`` / ``T&&`` family.
* **The ``[[unsafe]]`` fence** — the only spelling that opens an unsafe region.
* **fast_io** — the safe runtime and testbed; its macro bridge keys on the
  individual feature macros, never on the umbrella.

Toggles and feature macros
==========================

Every feature has its own ``-f`` flag and is usable on its own. ``-fsafecxx`` is
an *umbrella* that enables the whole bundle; it is not itself a language feature.

.. list-table::
   :header-rows: 1

   * - Feature
     - Toggle
     - Detection
   * - Herbceptions
     - ``-fherbceptions``
     - ``__HERBCEPTIONS__``
   * - Parameter passing (``in``/``out``/``inout``/``move``/``destructive_move``)
     - ``-fparameter-passing``
     - ``__PARAMETER_PASSING__``
   * - Lifetime protocol checking
     - ``-flifetime-protocol``
     - ``__LIFETIME_PROTOCOL__``
   * - Unsafe fence
     - ``-fsafecxx-unsafe``
     - ``__has_cpp_attribute(unsafe)``
   * - Umbrella: safe C++
     - ``-fsafecxx``
     - ``__SAFECXX__``

Notes:

* ``-fparameter-passing`` must not depend on the umbrella: ``in``/``out``/``move``
  are useful in ordinary C++ translation units.
* ``-fsafecxx-unsafe`` enables **only** the ``[[unsafe]]`` attribute, nothing
  else. Without it and without ``-fsafecxx``, ``__has_cpp_attribute(unsafe)``
  reports ``0`` — exactly as on compilers that do not implement safe C++.
* ``-fsafecxx`` does **not** imply ``-fno-exceptions`` or ``-fno-rtti``. Compiler
  and codegen behavior is unchanged; the restrictions below are *source-level*
  rules.

The ``[[unsafe]]`` fence
========================

Only the **block** form creates an unsafe region. An attribute on a declaration
is something else entirely:

.. list-table::
   :header-rows: 1

   * - Spelling
     - Meaning
   * - ``[[unsafe]] { ... }``
     - An unsafe region: raw pointers, references and iterators may be used
       inside. At namespace scope the block is scope-transparent and is how a
       legacy ``#include`` is fenced.
   * - ``[[unsafe]] void f() noexcept;``
     - A **call restriction**: ``f`` may be called only from inside an unsafe
       region. It says nothing about ``f``'s body.

Putting ``[[unsafe]]`` on a function does not make its body unsafe. If the body
uses unsafe features it needs its own block:

.. code-block:: c++

   [[unsafe]] void f() noexcept;          // unsafe to call

   void caller() noexcept {
     f();                                 // error: unsafe to call
     [[unsafe]] { f(); }                  // OK
   }

   [[unsafe]] void g() noexcept {
     [[unsafe]] {                         // required — body is not implicitly unsafe
       int *p = get_pointer();
       use(p);
     }
   }

The only exception is when the whole definition already sits inside a larger
fence (for example a namespace-scope block around a legacy header); that outer
region covers the body.

The fence is a source-level permission marker, exactly like it is for pointers:
it changes *where a construct may appear*, never what code is generated. Legacy
``try``/``catch`` and RTTI inside ``[[unsafe]] { }`` compile exactly as they do
in ordinary C++.

Dual compilation
----------------

``[[unsafe]]`` is a no-op for ordinary compilers — it is an unknown, ignored
attribute. A fence is therefore written once and guarded for dual-compilation
files:

.. code-block:: c++

   #ifdef __SAFECXX__
   [[unsafe]] {
   #endif
   #include "legacy_header.h"
   #ifdef __SAFECXX__
   }
   #endif

Restrictions in safe source
===========================

Outside a fence, safe source obeys these source-level rules:

* **No raw pointers, language references or iterators.** Iteration is by index;
  node-based containers are unsafe-only.
* **No legacy exceptions or RTTI.** ``throw expr``, ``try``, ``catch``,
  ``dynamic_cast`` and ``typeid`` are hard errors. Herbceptions are the only
  error channel; ``throw throws`` and ``catch throws`` are herbception spellings,
  not legacy exceptions.
* **Every function declares its error channel** — ``noexcept`` or ``throws``.
  There is no implicit "may throw" state.
* **Every virtual function is explicitly marked** ``virtual``, ``override`` or
  ``final``.
* **Every function declares its linkage** — ``static`` (or unnamed namespace),
  ``inline`` or ``extern``.
* **A destructor forces explicit copy operations** (at least ``= delete``).

Parameter passing
=================

``in``, ``out``, ``inout``, ``move`` and ``destructive_move`` are **contextual
keywords**, recognized only in parameter and return-type positions (the
``override``/``final`` precedent). They are never reserved globally, so
``int in = 0;``, a variable named ``out``, and ``std::move`` keep working —
enabling ``-fparameter-passing`` cannot break existing code. When recognized,
the declaration carries the new ABI/mangling, like ``throws``.

The parameter kinds also declare the crossing convention at the signature
instead of inferring it from a type's triviality. This *partially* mitigates the
long-standing Windows x64/MSVC by-value ABI divergence for move-only owners
(``unique_ptr``-like) and ``span``: only parameters (and allowed return
positions) in safe code are covered. Locals, members, container elements,
by-value returns and calls into legacy code still follow the platform ABI.
Plain ``move T`` is excluded from this benefit because it leaves a live,
destructible source; only ``destructive_move T`` gets explicit relocation with
no source destructor.

Lifetime protocol
=================

The protocol uses a per-value provenance model with two marks:

* ``[[owner]]`` on a class: its pointer fields are owned, and its object fields
  derive from ``{this'}``.
* ``[[lifetime(this)]]`` on a constructor/assignment parameter: the object's
  fields derive from that parameter.

There is no ``[[view]]`` attribute, no contracts, no GSL surface and no
type-shape inference. Assignment *replaces* a field's provenance set; it never
accumulates. In short: a value is valid when every owner in its provenance is
alive and has not been non-const-used.

Interaction with Clang's lifetime analysis
-------------------------------------------

Clang's existing lifetime-safety engine may be reused internally, but none of
its GSL or contract facilities are exposed as public surface. Its diagnostics
are silent for code that has not opted in (``§4d`` of ``DESIGN.md``).

See also
========

* :ref:`herbceptions` — the deterministic error channel.
* ``DESIGN.md`` in the ``safecppdesign`` repository — the complete proposal and
  the open decisions (D1–D23).
