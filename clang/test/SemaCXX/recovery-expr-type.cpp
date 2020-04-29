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
