//===- unittests/AST/ProfilingTest.cpp --- Tests for Profiling ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"
#include <utility>

namespace clang {
namespace {
using namespace ast_matchers;

TEST(Profiling, HerbceptionExpressionChildrenHaveLinearProfiles) {
  auto AST = tooling::buildASTFromCode("");
  ASTContext &Ctx = AST->getASTContext();
  auto MakeLiteral = [&Ctx](unsigned Value) {
    return IntegerLiteral::Create(Ctx, llvm::APInt(32, Value), Ctx.IntTy,
                                  SourceLocation());
  };
  auto ProfileSize = [&Ctx](Expr *E) {
    llvm::FoldingSetNodeID ID;
    E->Profile(ID, Ctx, /*Canonical=*/true);
    return ID.Intern(Ctx.getAllocator()).getSize();
  };

  for (unsigned Kind = 0; Kind != 3; ++Kind) {
    Expr *E = MakeLiteral(1);
    size_t Sizes[4] = {ProfileSize(E)};
    for (unsigned Depth = 1; Depth != 4; ++Depth) {
      if (Kind == 0)
        E = new (Ctx) CXXErrorValueExpr(E, MakeLiteral(2), MakeLiteral(3),
                                        Ctx.IntTy, SourceLocation());
      else if (Kind == 1)
        E = new (Ctx) CXXTryExpr(E, Ctx.IntTy, SourceLocation(), VK_PRValue,
                                 nullptr, nullptr, nullptr, QualType(),
                                 CXXTryExpr::PropagationKind::Raw);
      else
        E = new (Ctx) CXXCatchReturnFailureExpr(E, Ctx.IntTy, SourceLocation());
      Sizes[Depth] = ProfileSize(E);
    }

    // Each wrapper contributes a fixed amount of metadata and visits each
    // child exactly once. A second manual child walk duplicates progressively
    // deeper subtrees, so its profile-size increment cannot remain constant.
    EXPECT_EQ(Sizes[2] - Sizes[1], Sizes[1] - Sizes[0]) << Kind;
    EXPECT_EQ(Sizes[3] - Sizes[2], Sizes[1] - Sizes[0]) << Kind;
  }
}

static auto getClassTemplateRedecls() {
  std::string Code = R"cpp(
    template <class> struct A;
    template <class> struct A;
    template <class> struct A;
  )cpp";
  auto AST = tooling::buildASTFromCode(Code);
  ASTContext &Ctx = AST->getASTContext();

  auto MatchResults = match(classTemplateDecl().bind("id"), Ctx);
  SmallVector<ClassTemplateDecl *, 3> Res;
  for (BoundNodes &N : MatchResults) {
    if (auto *CTD = const_cast<ClassTemplateDecl *>(
            N.getNodeAs<ClassTemplateDecl>("id")))
      Res.push_back(CTD);
  }
  assert(Res.size() == 3);
#ifndef NDEBUG
  for (auto &&I : Res)
    assert(I->getCanonicalDecl() == Res[0]);
#endif
  return std::make_tuple(std::move(AST), Res[1], Res[2]);
}

template <class T> static void testTypeNode(const T *T1, const T *T2) {
  {
    llvm::FoldingSetNodeID ID1, ID2;
    T1->Profile(ID1);
    T2->Profile(ID2);
    ASSERT_NE(ID1, ID2);
  }
  auto *CT1 = cast<T>(T1->getCanonicalTypeInternal());
  auto *CT2 = cast<T>(T2->getCanonicalTypeInternal());
  {
    llvm::FoldingSetNodeID ID1, ID2;
    CT1->Profile(ID1);
    CT2->Profile(ID2);
    ASSERT_EQ(ID1, ID2);
  }
}

TEST(Profiling, DeducedTemplateSpecializationType_Name) {
  auto [AST, CTD1, CTD2] = getClassTemplateRedecls();
  ASTContext &Ctx = AST->getASTContext();

  auto *T1 = cast<DeducedTemplateSpecializationType>(
      Ctx.getDeducedTemplateSpecializationType(
          DeducedKind::Undeduced, /*DeducedAsType=*/QualType(),
          ElaboratedTypeKeyword::None, TemplateName(CTD1)));
  auto *T2 = cast<DeducedTemplateSpecializationType>(
      Ctx.getDeducedTemplateSpecializationType(
          DeducedKind::Undeduced, /*DeducedAsType=*/QualType(),
          ElaboratedTypeKeyword::None, TemplateName(CTD2)));
  testTypeNode(T1, T2);
}

} // namespace
} // namespace clang
