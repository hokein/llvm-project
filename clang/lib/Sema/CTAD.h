//===--- CTAD.h - Functions for CTAD implementations ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTMutationListener.h"
#include "clang/AST/ASTStructuralEquivalence.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/Template.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang {

FunctionTemplateDecl *buildDeductionGuide(
    Sema &SemaRef, TemplateDecl *OriginalTemplate,
    TemplateParameterList *TemplateParams, CXXConstructorDecl *Ctor,
    ExplicitSpecifier ES, TypeSourceInfo *TInfo, SourceLocation LocStart,
    SourceLocation Loc, SourceLocation LocEnd, bool IsImplicit,
    llvm::ArrayRef<TypedefNameDecl *> MaterializedTypedefs = {});

ParmVarDecl *transformFunctionTypeParam(
    Sema &SemaRef, ParmVarDecl *OldParam, MultiLevelTemplateArgumentList &Args,
    llvm::SmallVectorImpl<TypedefNameDecl *> &MaterializedTypedefs);

/// Transform a constructor template parameter into a deduction guide template
/// parameter, rebuilding any internal references to earlier parameters and
/// renumbering as we go.
NamedDecl *transformTemplateParameter(Sema &SemaRef, DeclContext *DC,
                                      NamedDecl *TemplateParam,
                                      MultiLevelTemplateArgumentList &Args,
                                      unsigned int UpdateIndex);

} // namespace clang