// RUN: %clang_cc1 -fsyntax-only -verify -frecovery-ast %s
//

int call(int); // expected-note {{'call' declared here}}

void test1(int s) {
  // verify no diagnostic "assigning to 'int' from incompatible type '<dependent type>'"
  s = call(); // expected-error {{too few arguments to function call}}
}

void test2() {
  // verify no diagnostic  "called object type '<dependent type>' is not a function or function pointer"
  static int ary3[(*__builtin_classify_type)(1)]; // expected-error {{builtin functions must be directly called}}
}
