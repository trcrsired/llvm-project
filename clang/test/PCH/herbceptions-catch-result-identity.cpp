// RUN: %clang_cc1 -x c++-header -std=c++20 -fherbceptions -emit-pch \
// RUN:   -o %t.pch %S/Inputs/herbceptions-catch-result-identity.h
// RUN: %clang_cc1 -std=c++20 -fherbceptions -include-pch %t.pch -verify %s

// The compiler-owned template specialization is recovered from the imported
// primary's lazy specialization set, rather than recreated from an empty
// ASTContext-local cache.
// The header deliberately creates the specialization through a typedef-sugar
// return type; this unsugared declaration must find the same canonical
// template-id after deserialization.
pch_tracked_value pch_make_unsugared() return_failure { int };
using fresh_catch_result = decltype(catch return_failure(pch_make_unsugared()));
static_assert(__is_same(imported_catch_result, fresh_catch_result));

// expected-no-diagnostics
