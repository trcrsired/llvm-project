// RUN: %clang_cc1 -fherbceptions -fsyntax-only -verify %s

struct Smaller {
  unsigned char value;
};

struct Larger {
  unsigned long long value[2];
};

struct SameSizeA {
  unsigned value;
};

struct SameSizeB {
  unsigned value;
};

_Static_assert(sizeof(struct Smaller) < sizeof(struct Larger), "");
_Static_assert(sizeof(struct SameSizeA) == sizeof(struct SameSizeB), "");

int reject_larger_as_smaller(struct Larger value)
    return_failure{struct Smaller} {
  return_failure value; // expected-error {{types must match exactly}}
}

int reject_smaller_as_larger(struct Smaller value)
    return_failure{struct Larger} {
  return_failure value; // expected-error {{types must match exactly}}
}

int reject_same_size_distinct(struct SameSizeB value)
    return_failure{struct SameSizeA} {
  return_failure value; // expected-error {{types must match exactly}}
}

typedef struct SameSizeA CanonicalAlias;

int accept_canonical_typedef(CanonicalAlias value)
    return_failure{struct SameSizeA} {
  return_failure value;
}

int accept_same_type_compound_literal(void)
    return_failure{struct SameSizeA} {
  return_failure (struct SameSizeA){17};
}
