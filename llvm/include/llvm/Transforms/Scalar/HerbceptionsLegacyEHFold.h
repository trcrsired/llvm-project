//===- HerbceptionsLegacyEHFold.h - fold legacy throws into herbceptions ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// When a legacy C++ throw (__cxa_throw / _CxxThrowException) provably unwinds
// into the compiler-generated legacy->std::error conversion site and nowhere
// else, the exception is only ever observed through the herbceptions error
// channel. This pass folds such throws into a direct call to the
// libherbceptions "direct" conversion entry points, eliminating the runtime
// unwind entirely.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_HERBCEPTIONSLEGACYEHFOLD_H
#define LLVM_TRANSFORMS_SCALAR_HERBCEPTIONSLEGACYEHFOLD_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class HerbceptionsLegacyEHFoldPass
    : public OptionalPassInfoMixin<HerbceptionsLegacyEHFoldPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_HERBCEPTIONSLEGACYEHFOLD_H
