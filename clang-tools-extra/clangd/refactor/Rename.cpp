//===--- Rename.cpp - Symbol-rename refactorings -----------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "refactor/Rename.h"
#include "Selection.h"
#include "AST.h"
#include "Logger.h"
#include "ParsedAST.h"
#include "SourceCode.h"
#include "FindTarget.h"
#include "index/SymbolCollector.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
#include "clang/Tooling/Refactoring/Rename/USRFinder.h"
#include "clang/Tooling/Refactoring/Rename/USRFindingAction.h"
#include "clang/Tooling/Refactoring/Rename/USRLocFinder.h"
#include <vector>

namespace clang {
namespace clangd {
namespace {

llvm::Optional<std::string> filePath(const SymbolLocation &Loc,
                                     llvm::StringRef HintFilePath) {
  if (!Loc)
    return None;
  auto Uri = URI::parse(Loc.FileURI);
  if (!Uri) {
    elog("Could not parse URI {0}: {1}", Loc.FileURI, Uri.takeError());
    return None;
  }
  auto U = URIForFile::fromURI(*Uri, HintFilePath);
  if (!U) {
    elog("Could not resolve URI {0}: {1}", Loc.FileURI, U.takeError());
    return None;
  }
  return U->file().str();
}

// Query the index to find some other files where the Decl is referenced.
llvm::Optional<std::string> getOtherRefFile(const Decl &D, StringRef MainFile,
                                            const SymbolIndex &Index) {
  RefsRequest Req;
  // We limit the number of results, this is a correctness/performance
  // tradeoff. We expect the number of symbol references in the current file
  // is smaller than the limit.
  Req.Limit = 100;
  if (auto ID = getSymbolID(&D))
    Req.IDs.insert(*ID);
  llvm::Optional<std::string> OtherFile;
  Index.refs(Req, [&](const Ref &R) {
    if (OtherFile)
      return;
    if (auto RefFilePath = filePath(R.Location, /*HintFilePath=*/MainFile)) {
      if (*RefFilePath != MainFile)
        OtherFile = *RefFilePath;
    }
  });
  return OtherFile;
}

enum ReasonToReject {
  NoSymbolFound,
  NoIndexProvided,
  NonIndexable,
  UsedOutsideFile,
  AmbiguousSymbol,
  UnsupportedSymbol,
};

// Check the symbol Decl is renameable (per the index) within the file.
llvm::Optional<ReasonToReject> renamableWithinFile(const Decl &RenameDecl,
                                                   StringRef MainFile,
                                                   const SymbolIndex *Index) {
  if (llvm::isa<NamespaceDecl>(&RenameDecl))
    return ReasonToReject::UnsupportedSymbol;
  if (const auto *FD = llvm::dyn_cast<FunctionDecl>(&RenameDecl)) {
    if (FD->isOverloadedOperator())
      return ReasonToReject::UnsupportedSymbol;
  }
  auto &ASTCtx = RenameDecl.getASTContext();
  const auto &SM = ASTCtx.getSourceManager();
  bool MainFileIsHeader = ASTCtx.getLangOpts().IsHeaderFile;
  bool DeclaredInMainFile = isInsideMainFile(RenameDecl.getBeginLoc(), SM);

  if (!DeclaredInMainFile)
    // We are sure the symbol is used externally, bail out early.
    return UsedOutsideFile;

  // If the symbol is declared in the main file (which is not a header), we
  // rename it.
  if (!MainFileIsHeader)
    return None;

  // Below are cases where the symbol is declared in the header.
  // If the symbol is function-local, we rename it.
  if (RenameDecl.getParentFunctionOrMethod())
    return None;

  if (!Index)
    return ReasonToReject::NoIndexProvided;

  bool IsIndexable = isa<NamedDecl>(RenameDecl) &&
                     SymbolCollector::shouldCollectSymbol(
                         cast<NamedDecl>(RenameDecl), ASTCtx, {}, false);
  // If the symbol is not indexable, we disallow rename.
  if (!IsIndexable)
    return ReasonToReject::NonIndexable;
  auto OtherFile = getOtherRefFile(RenameDecl, MainFile, *Index);
  // If the symbol is indexable and has no refs from other files in the index,
  // we rename it.
  if (!OtherFile)
    return None;
  // If the symbol is indexable and has refs from other files in the index,
  // we disallow rename.
  return ReasonToReject::UsedOutsideFile;
}

llvm::Error makeError(ReasonToReject Reason) {
  auto Message = [](ReasonToReject Reason) {
    switch (Reason) {
    case NoSymbolFound:
      return "there is no symbol at the given location";
    case NoIndexProvided:
      return "symbol may be used in other files (no index available)";
    case UsedOutsideFile:
      return "the symbol is used outside main file";
    case NonIndexable:
      return "symbol may be used in other files (not eligible for indexing)";
    case UnsupportedSymbol:
      return "symbol is not a supported kind (e.g. namespace, macro)";
    case AmbiguousSymbol:
      return "there are multiple symbols at the given location";
    }
    llvm_unreachable("unhandled reason kind");
  };
  return llvm::make_error<llvm::StringError>(
      llvm::formatv("Cannot rename symbol: {0}", Message(Reason)),
      llvm::inconvertibleErrorCode());
}

std::vector<const Decl *> getDeclAtPosition(ParsedAST &AST, Position P) {
  FileID FID;
  unsigned Offset;
  SourceLocation Pos;
  if (auto L = sourceLocationInMainFile(AST.getSourceManager(), P)) {
    Pos = *L;
  }
  std::tie(FID, Offset) = AST.getSourceManager().getDecomposedSpellingLoc(Pos);
  SelectionTree Selection(AST.getASTContext(), AST.getTokens(), Offset);
  std::vector<const Decl *> Result;
  
  if (const SelectionTree::Node *N = Selection.commonAncestor()) {
    // if (N->ASTNode.get<UsingDecl>()) {
    //   return {}; // doesn't support using decls.
    // }
    auto Decls = targetDecl(N->ASTNode,  DeclRelation::Alias | DeclRelation::TemplatePattern);
    Result.assign(Decls.begin(), Decls.end());
  }
  return Result;

}

llvm::DenseSet<const Decl*> getExtraRenameDecl(const NamedDecl* RenameDecl) {
  // RenajkkmeDecl = nullptr;
  if (llvm::isa_and_nonnull<CXXConstructorDecl>(RenameDecl))
    RenameDecl = dyn_cast<CXXConstructorDecl>(RenameDecl)->getParent();
  llvm::DenseSet<const Decl*> Results {RenameDecl->getCanonicalDecl()};
  if (auto *RD = dyn_cast<CXXRecordDecl>(RenameDecl)) {
    if (auto *CTD = RD->getDescribedClassTemplate()) {
      for (auto *S : CTD->specializations())
        Results.insert(S->getCanonicalDecl());
      SmallVector<ClassTemplatePartialSpecializationDecl *, 4> PartialSpecs;
      CTD->getPartialSpecializations(PartialSpecs);
      for (const auto *PD : PartialSpecs)
        Results.insert(PD->getCanonicalDecl());
    }
    for (const auto* CTR : RD->ctors())
      Results.insert(CTR->getCanonicalDecl());
  }
  if (auto *FD = dyn_cast<FunctionDecl>(RenameDecl)) {
    if (FD->getDescribedFunctionTemplate())
    for (auto *S : FD->getDescribedFunctionTemplate()->specializations())
      Results.insert(S->getCanonicalDecl());
  }
  if (auto *MD = dyn_cast<CXXMethodDecl>(RenameDecl)) {
    for (auto * OD : MD->overridden_methods())
      Results.insert(OD->getCanonicalDecl());
  }
  return Results;
}

// Return all rename occurrences in the main file.
// tooling::SymbolOccurrences
std::vector<SourceLocation>
findOccurrencesWithinFile(ParsedAST &AST, Position P, const NamedDecl *RenameDecl) {
  const NamedDecl *CanonicalRenameDecl =
      tooling::getCanonicalSymbolDeclaration(RenameDecl);
  assert(CanonicalRenameDecl && "RenameDecl must be not null");
  // std::vector<std::string> RenameUSRs =
  //     tooling::getUSRsForDeclaration(CanonicalRenameDecl, AST.getASTContext());
  // dyn_castCanonicalRenameDecl
  llvm::DenseSet<SymbolID> WhitelistIDs; 
  auto AllDecls = getExtraRenameDecl(RenameDecl);
  // for (const auto* D : getExtraRenameDecl(RenameDecl))
  //   if (auto ID = getSymbolID(D))
  //     WhitelistIDs.insert(*ID);
  // if (auto ID = getSymbolID(CanonicalRenameDecl))
  //   WhitelistIDs.insert(*ID);
  // if (const auto* R = dyn_cast<CXXRecordDecl>(CanonicalRenameDecl)) {
  //   if (auto ID = getSymbolID(R->getDestructor()))
  //      WhitelistIDs.insert(*ID);
  //   for (const auto* C : R->ctors())
  //   WhitelistIDs.insert(*getSymbolID(C));
  // }
  // CanonicalRenameDecl->dump();
  // for (const auto& USR : RenameUSRs) {
  //   llvm::errs() << "USR: " << USR << "\n";
  //   WhitelistIDs.insert(SymbolID(USR));
  // }
  // auto Decls = getDeclAtPosition(AST, P);
  // for (const auto* D : Decls) {
  //   if (auto ID = getSymbolID(D))
  //     WhitelistIDs.insert(*ID);
  // }
  std::string OldName = CanonicalRenameDecl->getNameAsString();
  tooling::SymbolOccurrences Result;
  // void findExplicitReferences(const Decl *D,
  //                           llvm::function_ref<void(ReferenceLoc)> Out);
  std::vector<SourceLocation> RenameRefs;
  for (Decl *TopLevelDecl : AST.getLocalTopLevelDecls())
  {
    findExplicitReferences(TopLevelDecl,
                           [&](ReferenceLoc Ref) {
                             if (Ref.Targets.size() != 1)
                               return;
                              if (AllDecls.find(Ref.Targets.front()->getCanonicalDecl()) != AllDecls.end())
                                RenameRefs.push_back(Ref.NameLoc);
                            //  if (auto ID = getSymbolID(Ref.Targets.front()))
                            //   if (WhitelistIDs.find(*ID) != WhitelistIDs.end())
                            // //  if (CanonicalRenameDecl->getCanonicalDecl() == Ref.Targets.front()->getCanonicalDecl())
                            //    RenameRefs.push_back(Ref.NameLoc);
                           });

    // tooling::SymbolOccurrences RenameInDecl =
    //     tooling::getOccurrencesOfUSRs(RenameUSRs, OldName, TopLevelDecl);
    // Result.insert(Result.end(), std::make_move_iterator(RenameInDecl.begin()),
    //               std::make_move_iterator(RenameInDecl.end()));
  }
  return RenameRefs;
}

} // namespace

llvm::Expected<tooling::Replacements>
renameWithinFile(ParsedAST &AST, llvm::StringRef File, Position Pos,
                 llvm::StringRef NewName, const SymbolIndex *Index) {
  const SourceManager &SM = AST.getSourceManager();
  SourceLocation SourceLocationBeg = SM.getMacroArgExpandedLocation(
      getBeginningOfIdentifier(Pos, SM, AST.getASTContext().getLangOpts()));
  // FIXME: renaming macros is not supported yet, the macro-handling code should
  // be moved to rename tooling library.
  if (locateMacroAt(SourceLocationBeg, AST.getPreprocessor()))
    return makeError(UnsupportedSymbol);

  const auto *RenameDecl =
      tooling::getNamedDeclAt(AST.getASTContext(), SourceLocationBeg);
  auto Decls = getDeclAtPosition(AST, Pos);
  if (Decls.size() > 1)
    return makeError(AmbiguousSymbol);
  if (Decls.empty())
    return makeError(NoSymbolFound);
  RenameDecl = llvm::dyn_cast<NamedDecl>(Decls.front()->getCanonicalDecl());
  // RenameDecl->dump();
  if (!RenameDecl)
    return makeError(UnsupportedSymbol);

  if (auto Reject =
          renamableWithinFile(*RenameDecl, File, Index))
    return makeError(*Reject);

  // Rename sometimes returns duplicate edits (which is a bug). A side-effect of
  // adding them to a single Replacements object is these are deduplicated.
  tooling::Replacements FilteredChanges;
  for (const SourceLocation RenameLoc :
       findOccurrencesWithinFile(AST,Pos, RenameDecl)) {
    // Currently, we only support normal rename (one range) for C/C++.
    // FIXME: support multiple-range rename for objective-c methods.
    // if (Rename.getNameRanges().size() > 1)
    //   continue;
    // We shouldn't have conflicting replacements. If there are conflicts, it
    // means that we have bugs either in clangd or in Rename library, therefore
    // we refuse to perform the rename.
    if (auto Err = FilteredChanges.add(tooling::Replacement(
            AST.getASTContext().getSourceManager(),
            CharSourceRange::getTokenRange(RenameLoc),
            NewName)))
      return std::move(Err);
  }
  return FilteredChanges;
}

} // namespace clangd
} // namespace clang
