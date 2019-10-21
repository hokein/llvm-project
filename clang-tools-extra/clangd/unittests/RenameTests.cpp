//===-- RenameTests.cpp -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Annotations.h"
#include "TestFS.h"
#include "TestTU.h"
#include "refactor/Rename.h"
#include "clang/Tooling/Core/Replacement.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace clang {
namespace clangd {
namespace {

MATCHER_P2(RenameRange, Code, Range, "") {
  return replacementToEdit(Code, arg).range == Range;
}
TEST(RenameTest, ClangRenameTest) {
  StringRef Tests[] = {
    R"cpp(
      class [[F^oo]] {};
      template <typename T> void func() {}
      template <typename T> class Baz {};
      int main() {
        func<[[F^oo]]>();             
        Baz<[[F^oo]]> obj;
        return 0;
      }
    )cpp",
    
    // class simple rename.
    R"cpp(
      class [[F^oo]] {
        void foo(int x);
      };
 
      void [[Foo]]::foo(int x) {}
    )cpp",
    // class overrides
    R"cpp(
      struct A {
       virtual void [[f^oo]]() {} 
      };
 
      struct B : A {
        void [[f^oo]]() override {}
      };
 
      struct C : B {
        void [[f^oo]]() override {}
      };
 
      struct D : B {
        void [[f^oo]]() override {}
      };
 
      struct E : D {
        void [[f^oo]]() override {} 
      };
 
      void func() {
        A a;
        a.[[foo]]();                           
        B b;
        b.[[foo]](); 
        C c;
        c.[[foo]]();
        D d;
        d.[[foo]]();
        E e;
        e.[[foo]]();
      }
    )cpp",
    // complicated class type.
    R"cpp(
      // Forward declaration.
      class [[Fo^o]];
 
      class Baz {
        virtual int getValue() const = 0;
      };
 
      class [[F^oo]] : public Baz  {
      public:
        [[Foo]](int value = 0) : x(value) {}
      
        [[Foo]] &operator++(int) {
          x++;
          return *this;
        }
 
        bool operator<([[Foo]] const &rhs) {
          return this->x < rhs.x;
        }
 
        int getValue() const {
          return 0;
        }
 
      private:
        int x;
      };
 
      void func() {
        [[Foo]] *Pointer = 0;
        [[Foo]] Variable = [[Foo]](10);
        for ([[Foo]] it; it < Variable; it++) {
        }
        const [[Foo]] *C = new [[Foo]]();
        const_cast<[[Foo]] *>(C)->getValue();
        [[Foo]] foo;
        const Baz &BazReference = foo;
        const Baz *BazPointer = &foo;
        dynamic_cast<const [[^Foo]] &>(BazReference).getValue();
        dynamic_cast<const [[^Foo]] *>(BazPointer)->getValue();
        reinterpret_cast<const [[^Foo]] *>(BazPointer)->getValue();
        static_cast<const [[^Foo]] &>(BazReference).getValue();
        static_cast<const [[^Foo]] *>(BazPointer)->getValue();
      }
    )cpp",
    // class constructors
    R"cpp(
      class [[^Foo]] { 
       public:
         [[Foo]]();
      };
 
      [[Foo]]::[[Fo^o]]() {}
    )cpp",
    // constructor initializer list.
    R"cpp(
      class Baz {};
      class Qux {
        Baz [[F^oo]];
      public:
        Qux();
      };
 
      Qux::Qux() : [[F^oo]]() {}
    )cpp",
 
    // DeclRef Expr?
    R"cpp(
      class C {
       public:
         static int [[F^oo]];
       };
 
       int foo(int x) { return 0; }
       #define MACRO(a) foo(a)
 
       void func() {
         C::[[F^oo]] = 1;
         MACRO(C::[[Foo]]);
         int y = C::[[F^oo]];
       }
    )cpp",
    // Forward declaration.
    R"cpp(
      class [[F^oo]];
      [[Foo]] *f();
    )cpp",
    // function marco????
    R"cpp(
      #define moo foo           // CHECK: #define moo macro_function
 
int foo() /* Test 1 */ {  // CHECK: int macro_function() /* Test 1 */ {
  return 42;
}
 
void boo(int value) {}
 
void qoo() {
  foo();                  // CHECK: macro_function();
  boo(foo());             // CHECK: boo(macro_function());
  moo();
  boo(moo());
}
    )cpp",
 
    R"cpp(
      class Baz {
       public:
         int [[Foo]];
       };
 
       int qux(int x) { return 0; }
       #define MACRO(a) qux(a)
 
       int main() {
         Baz baz;
         baz.[[Foo]] = 1;
         MACRO(baz.[[Foo]]);
         int y = baz.[[Foo]];
       }
    )cpp",
    
    // template class instantiation.
    R"cpp(
      template <typename T>
      class [[F^oo]] {
      public:
        T foo(T arg, T& ref, T* ptr) {
          T value;
          int number = 42;
          value = (T)number;
          value = static_cast<T>(number);
          return value;
        }
        static void foo(T value) {}
        T member;
      };
 
      template <typename T>
      void func() {
        [[F^oo]]<T> obj;
        obj.member = T();
        [[Foo]]<T>::foo();
      }
 
      int main() {
        [[F^oo]]<int> i;
        i.member = 0;
        [[F^oo]]<int>::foo(0);
 
        [[F^oo]]<bool> b;
        b.member = false;
        [[Foo]]<bool>::foo(false);
 
        return 0;
      }
    )cpp",
    // template arguments
    R"cpp(
      template <typename [[^T]]>
      class Foo {
        [[T]] foo([[T]] arg, [[T]]& ref, [[^T]]* ptr) {
          [[T]] value;
          int number = 42;
          value = ([[T]])number;
          value = static_cast<[[^T]]>(number); 
          return value;
        }
        static void foo([[T]] value) {}
        [[T]] member;
      };
    )cpp",
    // template class methods.
    R"cpp(
      template <typename T>
      class A {
      public:
        void [[f^oo]]() {}
      };
 
      void func() {
        A<int> a;
        A<double> b;
        A<float> c;
        a.[[f^oo]](); 
        b.[[f^oo]](); 
        c.[[f^oo]]();
      }
    )cpp",
 
    // Typedef.
    R"cpp(
      namespace std {
      class basic_string {};
      typedef basic_string [[s^tring]];
      } // namespace std
 
      std::[[s^tring]] foo();
    )cpp",
    // Variable.
    R"cpp(
      #define NAMESPACE namespace A
      NAMESPACE {
      int [[F^oo]];
      }
      int Foo;
      int Qux = Foo;
      int Baz = A::[[^Foo]];
      void fun() {
        struct {
          int Foo;
        } b = {100};
        int Foo = 100;
        Baz = Foo;
        {
          extern int Foo;
          Baz = Foo;
          Foo = A::[[F^oo]] + Baz;
          A::[[Fo^o]] /* Test 4 */ = b.Foo;
        }
        Foo = b.Foo;
      }
    )cpp",
  };
  int count = 0;
  for (const auto& Test : Tests) {
    Annotations Code(Test);
    for (const auto& RenamePos : Code.points()) {
      auto TU = TestTU::withCode(Code.code());
      auto AST = TU.build();
      auto RenameResult =
        renameWithinFile(AST, testPath(TU.Filename), RenamePos, "dummy");
      EXPECT_TRUE(bool(RenameResult)) << "renameWithinFile returned an error: "
                                 << llvm::toString(RenameResult.takeError());
      std::vector<testing::Matcher<tooling::Replacement>> Expected;
      for (const auto &R : Code.ranges())
        Expected.push_back(RenameRange(TU.Code, R));
      EXPECT_THAT(*RenameResult, UnorderedElementsAreArray(Expected)) << Test;
    }
    ++count;
    if (count > 10)
     break;
  }
}

TEST(RenameTest, SingleFile) {
  struct Test {
    const char* Before;
    const char* After;
  } Tests[] = {
      // Rename function.
      {
          R"cpp(
            void foo() {
              fo^o();
            }
          )cpp",
          R"cpp(
            void abcde() {
              abcde();
            }
          )cpp",
      },
      // Rename type.
      {
          R"cpp(
            struct foo{};
            foo test() {
               f^oo x;
               return x;
            }
          )cpp",
          R"cpp(
            struct abcde{};
            abcde test() {
               abcde x;
               return x;
            }
          )cpp",
      },
      // Rename variable.
      {
          R"cpp(
            void bar() {
              if (auto ^foo = 5) {
                foo = 3;
              }
            }
          )cpp",
          R"cpp(
            void bar() {
              if (auto abcde = 5) {
                abcde = 3;
              }
            }
          )cpp",
      },
  };
  for (const Test &T : Tests) {
    Annotations Code(T.Before);
    auto TU = TestTU::withCode(Code.code());
    auto AST = TU.build();
    auto RenameResult =
        renameWithinFile(AST, testPath(TU.Filename), Code.point(), "abcde");
    ASSERT_TRUE(bool(RenameResult)) << RenameResult.takeError();
    auto ApplyResult =
        tooling::applyAllReplacements(Code.code(), *RenameResult);
    ASSERT_TRUE(bool(ApplyResult)) << ApplyResult.takeError();

    EXPECT_EQ(T.After, *ApplyResult) << T.Before;
  }
}

TEST(RenameTest, Renameable) {
  struct Case {
    const char *Code;
    const char* ErrorMessage; // null if no error
    bool IsHeaderFile;
    const SymbolIndex *Index;
  };
  TestTU OtherFile = TestTU::withCode("Outside s; auto ss = &foo;");
  const char *CommonHeader = R"cpp(
    class Outside {};
    void foo();
  )cpp";
  OtherFile.HeaderCode = CommonHeader;
  OtherFile.Filename = "other.cc";
  // The index has a "Outside" reference and a "foo" reference.
  auto OtherFileIndex = OtherFile.index();
  const SymbolIndex *Index = OtherFileIndex.get();

  const bool HeaderFile = true;
  Case Cases[] = {
      {R"cpp(// allow -- function-local
        void f(int [[Lo^cal]]) {
          [[Local]] = 2;
        }
      )cpp",
       nullptr, HeaderFile, Index},

      {R"cpp(// allow -- symbol is indexable and has no refs in index.
        void [[On^lyInThisFile]]();
      )cpp",
       nullptr, HeaderFile, Index},

      {R"cpp(// disallow -- symbol is indexable and has other refs in index.
        void f() {
          Out^side s;
        }
      )cpp",
       "used outside main file", HeaderFile, Index},

      {R"cpp(// disallow -- symbol is not indexable.
        namespace {
        class Unin^dexable {};
        }
      )cpp",
       "not eligible for indexing", HeaderFile, Index},

      {R"cpp(// disallow -- namespace symbol isn't supported
        namespace fo^o {}
      )cpp",
       "not a supported kind", HeaderFile, Index},

      {
          R"cpp(
         #define MACRO 1
         int s = MAC^RO;
       )cpp",
          "not a supported kind", HeaderFile, Index},

      {

          R"cpp(
        struct X { X operator++(int) {} };
        void f(X x) {x+^+;})cpp",
          "not a supported kind", HeaderFile, Index},

      {R"cpp(// foo is declared outside the file.
        void fo^o() {}
      )cpp", "used outside main file", !HeaderFile /*cc file*/, Index},

      {R"cpp(
         // We should detect the symbol is used outside the file from the AST.
         void fo^o() {})cpp",
       "used outside main file", !HeaderFile, nullptr /*no index*/},
  };

  for (const auto& Case : Cases) {
    Annotations T(Case.Code);
    TestTU TU = TestTU::withCode(T.code());
    TU.HeaderCode = CommonHeader;
    if (Case.IsHeaderFile) {
      // We open the .h file as the main file.
      TU.Filename = "test.h";
      // Parsing the .h file as C++ include.
      TU.ExtraArgs.push_back("-xobjective-c++-header");
    }
    auto AST = TU.build();

    auto Results = renameWithinFile(AST, testPath(TU.Filename), T.point(),
                                    "dummyNewName", Case.Index);
    bool WantRename = true;
    if (T.ranges().empty())
      WantRename = false;
    if (!WantRename) {
      assert(Case.ErrorMessage && "Error message must be set!");
      EXPECT_FALSE(Results) << "expected renameWithinFile returned an error: "
                            << T.code();
      auto ActualMessage = llvm::toString(Results.takeError());
      EXPECT_THAT(ActualMessage, testing::HasSubstr(Case.ErrorMessage));
    } else {
      EXPECT_TRUE(bool(Results)) << "renameWithinFile returned an error: "
                                 << llvm::toString(Results.takeError());
      std::vector<testing::Matcher<tooling::Replacement>> Expected;
      for (const auto &R : T.ranges())
        Expected.push_back(RenameRange(TU.Code, R));
      EXPECT_THAT(*Results, UnorderedElementsAreArray(Expected));
    }
  }
}

} // namespace
} // namespace clangd
} // namespace clang
