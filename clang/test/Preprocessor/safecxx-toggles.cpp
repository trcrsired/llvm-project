// RUN: %clang -fsafecxx -dM -E -x c++ %s | FileCheck %s --check-prefix=ALL
// RUN: %clang_cc1 -fsafecxx -dM -E -x c++ %s | FileCheck %s --check-prefix=CC1ALL
// RUN: %clang -dM -E -x c++ %s | FileCheck %s --check-prefix=NONE
// RUN: %clang -fparameter-passing -dM -E -x c++ %s | FileCheck %s --check-prefix=PARAM
// RUN: %clang -flifetime-protocol -dM -E -x c++ %s | FileCheck %s --check-prefix=LIFETIME
// RUN: %clang -fsafecxx -fno-parameter-passing -dM -E -x c++ %s | FileCheck %s --check-prefix=OVERRIDE
// RUN: %clang_cc1 -fsafecxx -fno-parameter-passing -dM -E -x c++ %s | FileCheck %s --check-prefix=CCOVERRIDE

// The umbrella enables every individual feature macro.
// ALL-DAG: #define __HERBCEPTIONS__ 1
// ALL-DAG: #define __PARAMETER_PASSING__ 1
// ALL-DAG: #define __LIFETIME_PROTOCOL__ 1
// ALL-DAG: #define __SAFECXX__ 1
// CC1ALL-DAG: #define __HERBCEPTIONS__ 1
// CC1ALL-DAG: #define __PARAMETER_PASSING__ 1
// CC1ALL-DAG: #define __LIFETIME_PROTOCOL__ 1
// CC1ALL-DAG: #define __SAFECXX__ 1

// -fsafecxx deliberately does NOT imply -fno-exceptions or -fno-rtti:
// the default compiler behavior is unchanged. Use __cpp_exceptions, the
// standard feature macro (this fork's __EXCEPTIONS spelling omits trailing
// underscores, which is an unrelated pre-existing typo).
// ALL-DAG: #define __cpp_exceptions 199711L
// ALL-DAG: #define __GXX_RTTI 1

// Without any flag, none of the feature macros are defined.
// NONE-NOT: #define __HERBCEPTIONS__
// NONE-NOT: #define __PARAMETER_PASSING__
// NONE-NOT: #define __LIFETIME_PROTOCOL__
// NONE-NOT: #define __SAFECXX__

// Individual toggles work without the umbrella.
// PARAM-DAG: #define __PARAMETER_PASSING__ 1
// PARAM-NOT: #define __SAFECXX__
// PARAM-NOT: #define __LIFETIME_PROTOCOL__
// LIFETIME-DAG: #define __LIFETIME_PROTOCOL__ 1
// LIFETIME-NOT: #define __SAFECXX__
// LIFETIME-NOT: #define __PARAMETER_PASSING__

// An explicit -fno- wins over the umbrella, for both driver and cc1.
// OVERRIDE-NOT: #define __PARAMETER_PASSING__
// OVERRIDE: #define __SAFECXX__ 1
// CCOVERRIDE-NOT: #define __PARAMETER_PASSING__
// CCOVERRIDE: #define __SAFECXX__ 1
