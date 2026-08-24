#pragma once
#include "herbception-a.h"

// Cross-module reference: defined in herbc_b, calls a declaration from herbc_a.
inline int wrap_fail(int x) fails{int} {
  auto e = catch fails(fail_fn(x));
  return e.failed ? e.error : e.value;
}
