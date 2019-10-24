//===--- CollectMacros.h -----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_COLLECTEDMACROS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_COLLECTEDMACROS_H

#include "AST.h"
#include "Protocol.h"
#include "SourceCode.h"
#include "index/Ref.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include <memory>
#include <string>

namespace clang {
namespace clangd {

struct MainFileMacros {
  // struct Record {
  //   std::string Name;
  //   llvm::Optional<SymbolID> ID;
  //   std::vector<Range> Ranges;
  // };
  // std::vector<Record> Records;
  llvm::StringSet<> Names;
  // Instead of storing SourceLocation, we have to store the token range because
  // SourceManager from preamble is not available when we build the AST.
  std::vector<Range> Ranges;
  // clangd::RefSlab Refs;
  llvm::DenseMap<SymbolID, std::vector<Range>> Refs;
};

/// Collects macro references (e.g. definitions, expansions) in the main file.
/// It is used to:
///  - collect macros in the preamble section of the main file (in Preamble.cpp)
///  - collect macros after the preamble of the main file (in ParsedAST.cpp)
class CollectMainFileMacros : public PPCallbacks {
public:
  explicit CollectMainFileMacros(const SourceManager &SM,
                                 const LangOptions &LangOpts,
                                 std::shared_ptr<Preprocessor> PP,
                                 MainFileMacros &Out)
      : SM(SM), LangOpts(LangOpts), PP(std::move(PP)), Out(Out) {}

  void FileChanged(SourceLocation Loc, FileChangeReason,
                   SrcMgr::CharacteristicKind, FileID) override {
    InMainFile = isInsideMainFile(Loc, SM);
  }

  void MacroDefined(const Token &MacroName, const MacroDirective *MD) override {
    add(MacroName, MD->getMacroInfo());
    add(MacroName, MD? MD : nullptr);
  }

  void MacroExpands(const Token &MacroName, const MacroDefinition &MD,
                    SourceRange Range, const MacroArgs *Args) override {
    add(MacroName, MD? MD.getLocalDirective() : nullptr);
    add(MacroName, MD.getMacroInfo());
  }

  void MacroUndefined(const clang::Token &MacroName,
                      const clang::MacroDefinition &MD,
                      const clang::MacroDirective *Undef) override {
    add(MacroName, MD? MD.getLocalDirective() : nullptr);
    add(MacroName, MD.getMacroInfo());
    // if (MD.getMacroInfo()) {
    //   auto It = Refs.find(MacroName.getIdentifierInfo());
    //   if (It != Refs.end()) {
    //   if (auto ID = getSymbolID(*MacroName.getIdentifierInfo(), MD.getMacroInfo(), SM)) {
    //      llvm::errs() << "ID: " << *ID << "\n";
    //     //  MI->dump();
    //      Out.Refs[*ID] = std::move(It->getSecond());
    //      Refs.erase(It->getFirst());
    //    }
    //   }
    // }
    // add(MacroName, MD.getMacroInfo());
  }

  void Ifdef(SourceLocation Loc, const Token &MacroName,
             const MacroDefinition &MD) override {
    add(MacroName, MD? MD.getLocalDirective() : nullptr);
    add(MacroName, MD.getMacroInfo());
  }

  void Ifndef(SourceLocation Loc, const Token &MacroName,
              const MacroDefinition &MD) override {
    add(MacroName, MD? MD.getLocalDirective() : nullptr);
    add(MacroName, MD.getMacroInfo());
  }

  void Defined(const Token &MacroName, const MacroDefinition &MD,
               SourceRange Range) override {
                 MD.getMacroInfo();
    add(MacroName, MD? MD.getLocalDirective() : nullptr);
    add(MacroName, MD.getMacroInfo());
  }

  void EndOfMainFile() override {
    for (auto& M : MacroRecords) {
      const MacroInfo* S = nullptr;
      if (M.Macro)
        S = M.Macro->getMacroInfo();
      else if (PP){
        S = PP->getMacroInfo(M.II);
      }
      // if (!M.Macro)
      //   S = PP->getMacroInfo(M.II);
      // else {
      //   S = M.Macro->getMacroInfo();
      // }
      // if (M.Macro)
      //   S = M.Macro->getMacroInfo();
      // else {
      //   // S = PP->getMacroInfo(M.MacroName.getIdentifierInfo());
      // }
      if (S) {
        if (auto ID = getSymbolID(*M.II, S, SM))
          Out.Refs[*ID].push_back(M.Range);
      }

      // else {
        // Out.Ranges.push_back(M.Range);
      // }
    }
  }
  //   for (auto It : Refs) {
  //     if (!PP) break;
      
    //  if (auto MI = PP->getMacroInfo(It.getFirst())) {
  //      llvm::errs() << "??\n";
  //     //  llvm::errs() << It.getFirst() << ": " << It.getFirst()->getName() << "\n";
  //      if (auto ID = getSymbolID(*It.getFirst(), MI, SM)) {
  //        llvm::errs() << "ID: " << *ID << "\n";
  //        MI->dump();
  //        Out.Refs[*ID] = std::move(It.getSecond());
  //      }
  //    }
  //   }
  // }

private:
  struct MacroRecord {
    const IdentifierInfo* II;
    // const clang::Token MacroName;        ///< The spelling site for this macro.
    const clang::MacroDirective* Macro;  ///< The macro itself, if defined.
    // bool WasDefined;  ///< If true, the macro was defined at time of deferral.
    Range Range;  ///< The range covering the spelling site.
  };
  std::vector<MacroRecord> MacroRecords;

  void add(const Token &MacroNameTok, const MacroDirective *MI) {
    if (!InMainFile)
      return;
    auto Loc = MacroNameTok.getLocation();
    if (Loc.isMacroID())
      return;
    if (auto R = getTokenRange(SM, LangOpts, Loc)) {
      MacroRecords.push_back({MacroNameTok.getIdentifierInfo(), MI, *R});
    }
  }
  void add(const Token &MacroNameTok, const MacroInfo *MI) {
    if (!InMainFile)
      return;
    auto Loc = MacroNameTok.getLocation();
    if (Loc.isMacroID())
      return;
    // MacroNameTok.
    SymbolID NonDefinedMacroID("");
    if (auto Range = getTokenRange(SM, LangOpts, Loc)) {
      Out.Names.insert(MacroNameTok.getIdentifierInfo()->getName());
      Out.Ranges.push_back(*Range);
      if (MI) {
        llvm::errs() << "dump macro info\n";
        MI->dump();
      }
      // Refs[MI].push_back(*Range);
      Refs[MacroNameTok.getIdentifierInfo()].push_back(*Range);
      llvm::errs() << MacroNameTok.getIdentifierInfo() << ": " << MacroNameTok.getIdentifierInfo()->getName() << " " << *Range << "\n";
      if (!MI) {
        // Out.Refs[NonDefinedMacroID].push_back(*Range);
      }
      // if (auto ID = getSymbolID(*MacroNameTok.getIdentifierInfo(), MI, SM))
      //    Out.Refs[*ID].push_back(*Range);
    }
  }
  const SourceManager &SM;
  const LangOptions &LangOpts;
  bool InMainFile = true;
  llvm::DenseMap<const IdentifierInfo*, std::vector<Range>> Refs;
  std::shared_ptr<Preprocessor> PP;
  MainFileMacros &Out;
};

} // namespace clangd
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANGD_COLLECTEDMACROS_H
