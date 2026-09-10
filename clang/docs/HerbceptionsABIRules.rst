==========================================================
Herbceptions ABI Rules
==========================================================

.. contents::
   :local:

Overview
========

This document describes the ABI rules for Herbceptions, focusing on:

* payload size rules for ``T`` and ``E``;
* register vs. sret buffer return behavior;
* construction semantics of ``catch return_failure(expr)``;
* restrictions on ``T`` and ``E`` for ``return_failure{E}``.

These rules complement the main Herbceptions language document and define
the deterministic calling convention used by ``throws`` and
``return_failure{E}`` functions.

----------------------------------------------------------
ABI size rules for T and E
----------------------------------------------------------

Herbceptions lower a ``throws`` / ``return_failure{E}`` function into a
deterministic two-channel return ABI. The success payload ``T`` and the
error payload ``E`` share a single payload slot whose size is:

::

   max(sizeof(T), sizeof(E))

The discriminant is carried either in an ``i1`` IR bit or in a
target-specific flag (x86-64 ``CF``, AArch64 ``C``, etc.).

Register capacity
-----------------

On targets supporting ``supportThrowsCC()``, the payload may be returned in
registers if and only if:

::

   sizeof(T) <= R
   sizeof(E) <= R

where ``R`` is the target’s maximum register return capacity (typically
16 bytes on x86-64 and AArch64).

If both payloads fit, the ABI is:

* success payload in normal return registers;
* error payload in error registers;
* discriminant in carry flag (or extra register).

This is the zero-overhead path.

Payload larger than register capacity
-------------------------------------

If either payload exceeds ``R``, the caller must provide an sret buffer for
that payload. The callee writes the payload into the caller-allocated
buffer and sets the discriminant accordingly.

Four cases exist:

1. ``sizeof(T) <= R`` and ``sizeof(E) <= R``

   * no sret arguments
   * both payloads in registers
   * discriminant in CF / NZCV / a2

2. ``sizeof(T) <= R`` and ``sizeof(E) > R``

   * caller passes ``E* error_buffer``
   * success payload in registers
   * error payload written to ``*error_buffer``

3. ``sizeof(T) > R`` and ``sizeof(E) <= R``

   * caller passes ``T* success_buffer``
   * success payload written to ``*success_buffer``
   * error payload in registers

4. ``sizeof(T) > R`` and ``sizeof(E) > R``

   * caller passes both ``T* success_buffer`` and ``E* error_buffer``
   * success payload written to ``*success_buffer``
   * error payload written to ``*error_buffer``


Why two buffers?
----------------

A single buffer cannot be used for both payloads because:

* ``T`` and ``E`` may have different sizes;
* ``T`` and ``E`` may have different alignments;
* the caller must allocate ABI-correct storage;
* the callee cannot dynamically choose a layout;
* aliasing a shared buffer would pessimize optimization.

Therefore, each oversized payload requires its own sret pointer.

Equal-size oversized payloads
-----------------------------

When both ``T`` and ``E`` exceed the register return capacity ``R`` *and*
their sizes are equal:

::

   sizeof(T) > R
   sizeof(E) > R
   sizeof(T) == sizeof(E)

the caller passes **a single sret buffer**, not two.

In this case, the payload slot has size ``sizeof(T) == sizeof(E)``, and the
callee may construct either ``T`` or ``E`` directly into the same buffer.
The discriminant determines how the caller interprets the contents:

* on success: buffer contains ``T``;
* on failure: buffer contains ``E``.

This optimization reduces ABI complexity and avoids redundant pointer
arguments. It is safe because:

* ``T`` and ``E`` have identical size;
* both are trivially copyable;
* both satisfy the alignment requirements of the shared buffer;
* the caller reconstructs the N2289 aggregate based solely on the
  discriminant flag.

This rule applies only when ``sizeof(T) == sizeof(E)`` and both exceed the
register capacity. If the sizes differ, two buffers are required.

Call-site reconstruction
------------------------

Regardless of register vs sret return, the caller reconstructs the N2289
aggregate:

::

   struct {
     union { T value; E error; };
     bool failed;
   }

The aggregate is never returned by the callee; it is synthesized at the
call site from registers or sret buffers and the discriminant flag.

----------------------------------------------------------
Construction semantics of ``catch return_failure(expr)``
----------------------------------------------------------

A ``catch return_failure(expr)`` expression materializes the N2289 aggregate:

::

   struct {
     union { T value; E error; };
     bool failed;
   }

The payload slot has size ``max(sizeof(T), sizeof(E))`` and is always
allocated in the caller’s stack frame. The callee never constructs into
this slot.

Success case: constructing T
----------------------------

Case 1: ``sizeof(T) <= R`` (register-return)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* callee returns ``T`` in registers;
* caller copies ``T`` into the aggregate payload slot;
* copy is trivial (bytewise), not a constructor call.

Case 2: ``sizeof(T) > R`` (sret-return)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* caller passes ``T* success_buffer``;
* callee constructs ``T`` directly into ``success_buffer`` (RVO);
* caller copies ``T`` from ``success_buffer`` into the aggregate.

Failure case: constructing E
----------------------------

Case 1: ``sizeof(E) <= R`` (register-return)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* callee returns ``E`` in registers;
* caller copies ``E`` into the aggregate payload slot.

Case 2: ``sizeof(E) > R`` (sret-return)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* caller passes ``E* error_buffer``;
* callee constructs ``E`` directly into ``error_buffer``;
* caller copies ``E`` from ``error_buffer`` into the aggregate.

No double construction
----------------------

Only one of ``T`` or ``E`` is ever constructed by the callee, and only one
is ever copied by the caller. No temporary objects are created, and no
destructors or user-defined copy/move constructors are invoked.

All copies performed by ``catch return_failure`` are trivial bytewise copies.

Summary table
-------------

+----------+----------------------+------------------------------+------------------------------+
| Payload  | Fits in registers?   | Callee constructs where?     | Caller action                |
+==========+======================+==============================+==============================+
| T        | Yes                  | registers                    | copy registers → aggregate   |
+----------+----------------------+------------------------------+------------------------------+
| T        | No                   | ``T* success_buffer``        | copy buffer → aggregate      |
+----------+----------------------+------------------------------+------------------------------+
| E        | Yes                  | registers                    | copy registers → aggregate   |
+----------+----------------------+------------------------------+------------------------------+
| E        | No                   | ``E* error_buffer``          | copy buffer → aggregate      |
+----------+----------------------+------------------------------+------------------------------+

----------------------------------------------------------
Restrictions on T and E for ``return_failure{E}``
----------------------------------------------------------

The ``return_failure{E}`` specifier is a C-style deterministic error channel
intended for cross-language ABI interoperability. For this reason, both the
success type ``T`` and the error type ``E`` must be plain C types with
trivial semantics.

Triviality requirements
-----------------------

1. ``E`` must be trivially copyable.
2. ``T`` must also be trivially copyable.

The payload slot is a raw byte buffer of size ``max(sizeof(T), sizeof(E))``.
The caller must be able to trivially copy either payload into the aggregate
without invoking constructors, destructors, or move/copy operations.

Forbidden types
---------------

The following are **not allowed** as ``T`` or ``E``:

* classes with constructors or destructors;
* classes with copy/move constructors;
* classes with non-trivial assignment operators;
* classes with virtual functions or vtables;
* classes with non-trivial layout or alignment;
* classes with RAII behavior (``std::string``, ``std::vector``,
  ``std::unique_ptr``);
* classes with reference members;
* classes requiring cleanup or custom lifetime management;
* any non-POD type;
* any type requiring non-trivial initialization;
* any polymorphic type;
* any C++ exception type.

Allowed types
-------------

* scalars;
* enums;
* plain C structs;
* trivially copyable aggregates;
* POD types.

Rationale
---------

``return_failure{E}`` is designed for C ABI and cross-language FFI boundaries
(Rust, Go, Swift, Python, Fortran). These environments cannot support C++
object semantics such as constructors, destructors, vtables, or RAII.

Therefore:

::

   return_failure{E} is strictly a C ABI feature.
   T and E must be pure C types.
   No non-trivially-copyable C++ object is allowed.

