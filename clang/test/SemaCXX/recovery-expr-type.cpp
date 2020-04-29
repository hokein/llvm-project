// RUN: %clang_cc1 -triple=x86_64-unknown-unknown -frecovery-ast -frecovery-ast-type -o - %s -fsyntax-only -verify

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

namespace test1 {
constexpr int foo() { return 1;} // expected-note {{candidate function not viable}}
// verify the "not an integral constant expression" diagnostic is suppressed.
static_assert(1 == foo(1), ""); // expected-error {{no matching function}}
}

namespace test2 {
void foo(); // expected-note 2{{requires 0 arguments}}
class Y {
  // verify that "field has incomplete type" diagnostic is suppressed.
  typeof(foo(42)) var; // expected-error {{no matching function}}
  // FIXME: supporess the "invalid application" diagnostic.
  int s = sizeof(foo(42)); // expected-error {{no matching function}} expected-error {{invalid application of 'sizeof'}}
};
}

namespace test3 {
template <int N> constexpr int templated() __attribute__((enable_if(N, ""))) { // expected-note {{candidate disabled}}
  return 1;
}
// verify that "constexpr variable must be initialized" diagnostic is suppressed.
constexpr int A = templated<0>(); // expected-error{{no matching function}}
}
