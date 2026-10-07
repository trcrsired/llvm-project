.. _herbceptions-implementation:

==========================================================
Herbceptions Implementation Details
==========================================================

.. contents::
   :local:

.. note::

   This document covers the ``libherbceptions`` runtime side of the
   legacy-EH interop mechanism: the ``cxx_std_error_legacy_exception`` box,
   the query-information buffer ABI, and the platform exception-pointer
   domains. Compiler-side lowering topics (the ``{T, i1}`` channel, the
   discriminant conventions, mangling encodings) live elsewhere in the
   tree; the user-facing surface is documented in :ref:`herbceptions`.

Overview
--------

The interop mechanism lets a ``std::error`` cross the traditional C++
exception channel and come back onto the herbception channel with its
``{domain, code}`` payload intact. Three pieces cooperate:

* ``std::cxx_std_error_legacy_exception`` — a ``std::exception``-derived
  box thrown by ``error::throw_legacy_exception[_no_what]()`` when the
  domain's ``do_throw_legacy_exception`` hook is absent or declines.
* ``__cxa_error_query_information_{create,clone,free}`` — the runtime buffer
  ABI backing the box's cached ``what()`` text.
* The platform exception-pointer domain (itanium or MSVC), which boxes
  thrown objects as channel codes and recognizes our own box on the way
  back in, forwarding ``do_to_errc`` / ``do_equivalent`` /
  ``do_query_information`` into the carried payload.

The legacy-exception box
------------------------

``cxx_std_error_legacy_exception`` lives in
``include/herbceptions/__details/legacy_exception.h`` (whole file guarded
by ``__cpp_exceptions``). It stores three words: the domain singleton
pointer, the code, and a cached ``io_scatter_t`` message.

Ownership model
```````````````

The class has two construction modes:

* **Owning** — the public ``(domain, code)`` and ``cxx_std_error``
  constructors; the box runs ``do_cleanup`` on destruction.
* **Borrowed** — private ``__borrowed_t``-tagged constructor invoked
  exclusively by ``friend class std::error``. After optionally querying the
  message under the *original* domain, any code carrying a ``do_cleanup``
  hook is demoted through ``do_to_errc`` to the posix domain: a borrowed
  box must never share a cleanup obligation with the ``std::error`` it
  outlives — the same demotion applies on copy, where two owners of one
  cleanup-bearing code would double-clean.

Copies are ``noexcept`` (legacy EH requires copyable exceptions): they
clone the message buffer via ``__cxa_error_query_information_clone`` and
demote owned codes; moves steal both fields and reset the source.

There is no default constructor — the box always carries a payload.

Constexpr
`````````

Non-virtual members are unconditionally ``constexpr``. The destructor and
``what()`` are marked ``constexpr`` only when
``__cpp_constexpr_exceptions >= 202411L`` (P3068) is advertised — a
``constexpr`` virtual may only override a ``constexpr`` base, and current
standard libraries have not yet marked ``std::exception``'s members.
``__HERBCEPTIONS_CONSTEXPR_EXCEPTION`` is defined/undefined inside the
header to apply this.

Execution-charset ``what()``
````````````````````````````

``what()`` must produce text in the TU's narrow execution charset. The box
detects it at compile time, mirroring fast_io's
``execution_charset_encoding_scheme<char>``:

* ``static_cast<char8_t>('A') != u8'A'`` → ``utfebcdic``
* ``_MSVC_EXECUTION_CHARACTER_SET`` 936/54936 → ``gb18030``
* ``__GNUC_EXECUTION_CHARSET_NAME`` compared case-insensitively against
  ``"GB18030"``/``"GBK"`` → ``gb18030``
* otherwise ``utf8``

Fast path
`````````

``throw_legacy_exception_no_what()`` constructs the borrowed box with the
message query skipped: no ``do_query_information`` calls and no
allocation, ``what()`` returns ``""``. The ``{domain, code}`` payload is
identical, so the reboxed error keeps full semantics — only the cached
legacy-side message is skipped. When the compiler proves the unwind edge
folds back onto the channel, the trivial constructor leaves almost no code
behind.

Under ``-fno-exceptions`` both throw entry points call ``std::terminate()``
rather than being ill-formed, following P1105R0.

Query-information buffer ABI
----------------------------

``src/query_information.cpp`` implements the three entry points. The
buffer contract:

* ``len > 0`` — a heap buffer allocated inside the runtime
  (``__malloc_or_heap_alloc_or_die``) with a 4-byte NUL pad after the text,
  enough to terminate a string in the widest supported encoding (utf32).
* ``len == 0`` — a shared global ``char32_t{0}``; all-zero bytes terminate
  a string in every supported encoding (utf8/utf16/utf32/gb18030/EBCDIC).
  ``free()`` is a no-op on it, and callers must never pass it to free.

``create()`` drives the domain's ``do_query_information`` hook **once**
with an appendable cookie: every emit callback appends its scatter
fragments onto a growable buffer — the cookie is, in effect, a
``std::string`` — starting from 256 bytes and doubling (capacity always
reserves the 4-byte NUL pad; overflow terminates). A ``len == 0`` outcome
releases whatever was allocated and returns the shared empty buffer.
Allocation failure terminates via ``_or_die``.

Why ``__HERBCEPTIONS_API_ALLOC`` exists
```````````````````````````````````````

``__HERBCEPTIONS_API`` carries ``[[gnu::const]]`` on the consumer side —
correct for pure lookups where identical inputs genuinely produce
identical outputs. On allocation-returning entry points it would let the
compiler CSE two ``create`` calls into one pointer → double-free, and a
``const``-marked ``free`` could be dead-call-eliminated → leak. The
``_ALLOC`` sibling keeps the export attributes (visibility/weak/dllexport)
but drops the purity claim.

(``[[gnu::malloc(dealloc)]]`` was considered and rejected: it only applies
to pointer-returning functions, and these return ``io_scatter_t``.)

The exception-pointer domains
-----------------------------

Both platforms store the "boxed exception" as a raw code in the channel
and share one vtable shape; only the storage representation differs.

Itanium
```````

``src/itanium_exception_ptr.cpp``: the code is the **thrown-object
pointer** itself (``__cxa_exception_from_thrown_object`` recovers the
``__cxa_exception`` header at ``obj - 1``). Domain hooks:

* ``do_cleanup`` — ``__cxa_decrement_exception_refcount``.
* ``do_equivalent`` — compares the thrown-object pointer against the other
  code (with the same fast errc path when the other domain is posix).
* ``do_to_errc`` — catches-as ladder over known standard types via
  ``itanium_cxa_try_catch``; our own box is detected first and forwards to
  the carried domain. ``system_error``'s category is recognized by
  comparing ``category().name()`` against ``"generic"`` — deliberately not
  ``generic_category()``, which would drag the STL's category singleton
  into the TU.
* ``do_query_information`` — same box-detection forward, else
  ``"[itanium_exception](<demangled-or-mangled-name>)<what>"`` via
  ``__cxa_demangle``.
* ``do_throw_legacy_exception`` — ``_Unwind_RaiseException`` on the real
  runtime (GLIBCXX) or ``__cxa_rethrow_primary_exception`` elsewhere.

Catch-type matching uses ``__cxxabiv1::__class_type_info::__do_catch``
under libstdc++, falling back to a hand-rolled single-inheritance walk
(``rtti_si_derives_from``) on other runtimes — so
``catch (std::exception&)``-style base matches work without RTTI
vtable knowledge.

``__cxa_error_code_itanium_exception_ptr(flags, eh, tinfo, dtor)``:

* ``flags == 0`` — retain: ``__cxa_increment_exception_refcount`` and
  return the same pointer. This is also the ``exception_ptr`` decode: the
  code *is* the exception_ptr's ``void*`` payload, so
  ``make_exception_ptr_from_std_error`` retains then ``memcpy``s.
* ``flags == 1`` — in-flight capture (landing-pad/wasm.get.exception
  operand): ``__cxa_get_globals`` bookkeeping then retain.
* ``flags == 2`` — fabricated ``__cxa_init_primary_exception`` for
  folded unwind edges.

``__cxa_error_code_itanium_exception_ptr_cast(domain, code, ti)`` —
domain check, then ``itanium_cxa_try_catch`` against ``*ti`` returning the
adjusted subobject pointer.

MSVC
``````

``src/msvc_exception_ptr.cpp``: the code is a pointer to a heap box laid
out exactly like ``__std_exception_ptr`` — ``{void *rec, void *ref}`` —
allocated inside the runtime so ``__ExceptionPtrDestroy`` stays
correct across CRT boundaries. The first word is the ``EXCEPTION_RECORD*``,
whose ``ExceptionInformation[2]`` points at the ``_ThrowInfo`` and
``[3]`` at the module base for RVA resolution (x64).

Type matching walks the ``ThrowInfo``'s catchable-type table
(``cxx_type_info_table`` → ``cxx_type_info`` → ``_TypeDescriptor``),
comparing decorated names
(``try_get_cpp_exception_with_mangled_name``) and computing the adjusted
subobject through ``get_this_pointer`` — including virtual-base offsets.
``std::type_info`` *is* the ``_TypeDescriptor`` on this ABI, so the
caller-supplied RTTI pointer is reinterpreted as
``msvc_raw_type_info`` directly — no member calls on foreign objects.

``msvc_exception_kind`` is resolved by a gperf perfect-hash table over
the well-known decorated names (``.?AVsystem_error@std@@``,
``.?AVcxx_std_error_legacy_exception@std@@``, …): edit
``utils/msvc_exceptions_hash.gperf`` and regenerate
``src/msvc_exception_gperf`` with ``gperf`` — never hand-edit the
generated file. ``try_match_msvc_exceptions`` returns the kind plus the
adjusted object pointer for the kinds that need it.

``__cxa_error_code_msvc_exception_ptr(flags, a, b)``:

* ``flags == 0`` — ``__ExceptionPtrCopy`` an existing exception_ptr box
  (``a`` points at a ``__std_exception_ptr``-shaped object — including a
  channel code, which has that exact layout).
* ``flags == 1`` — ``__ExceptionPtrCurrentException`` (in-flight capture).
* ``flags == 2`` — ``__ExceptionPtrCopyException(a, b)`` for folded
  unwind edges (object + ``_ThrowInfo*``).

``__cxa_error_code_msvc_exception_ptr_cast`` — domain check, then the
catchable-name walk. ``__cxa_error_code_msvc_exception_ptr_unbox`` —
``__ExceptionPtrCopy`` into the caller's ``exception_ptr`` storage.

Symbol-dependency footprint
---------------------------

The MSVC TU pulls in only the ``__ExceptionPtr*`` CRT entry points, heap
allocation, and ``str*`` — no EH machinery. The itanium TU necessarily
references the ABI refcount functions (``__cxa_{increment,decrement}_
exception_refcount``, ``__cxa_init_primary_exception``,
``__cxa_rethrow_primary_exception``, ``__cxa_get_globals``), typeinfo
*data* (no code), and one 8-byte LSDA entry + ``__gxx_personality_v0``
for the ``noexcept`` terminate pad around non-noexcept ABI calls. Neither
drags ``generic_category`` or other ``<system_error>`` machinery.

Testing
-------

``test/legacy_exception_test.cpp`` covers: box payload + cached
``what()``, the no-query fast path, the double-hop
(``throws → throw_legacy_exception → catch throws``) forwarding, the
``exception_cast_std_error`` probe (exact type, public base,
negative, non-domain), and ``make_exception_ptr_from_std_error``.
