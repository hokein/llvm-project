// RUN: %clang_cc1 -triple=x86_64-unknown-unknown -frecovery-ast -frecovery-ast-type -std=c++11 -o - %s -fsyntax-only -verify

namespace NoCrash{
struct Indestructible {
  // Indestructible();
  ~Indestructible() = delete; // expected-note {{deleted}}
};
Indestructible make_indestructible();

// no crash on HasSideEffect.
void test() {
  int s = sizeof(make_indestructible()); // expected-error {{deleted}}
}
}

namespace static_assert_diags {
constexpr int foo() { return 1;} // expected-note {{candidate function not viable}}
// verify the "not an integral constant expression" diagnostic is suppressed.
static_assert(1 == foo(1), ""); // expected-error {{no matching function}}
}