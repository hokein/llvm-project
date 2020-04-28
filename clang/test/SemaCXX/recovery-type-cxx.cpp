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

namespace {
void foo(); // expected-note {{requires 0 arguments}}
class Y {
  // verify that "field has incomplete type" diagnostic is suppressed.
  typeof(foo(42)) invalid; // expected-error {{no matching function}}
};
}

namespace {
struct Incomplete; // expected-note 6{{forward declaration of}}
Incomplete make_incomplete(); // expected-note 3{{declared here}}
void test() {
  // FIXME: suppress the "member access" diagnostic.
  // FIXME：preserve the recovery-expr, right now clang drops them.
  make_incomplete().a; // expected-error {{incomplete}} expected-error {{member access into}}
  // FIXME: suppress the following "invalid application of 'sizeof'" diagnostic.
  sizeof(make_incomplete()); // expected-error {{calling 'make_incomplete' with incomplete return type}} expected-error {{invalid application of 'sizeof'}}
  // FIXME: suppress the "an incomplete type" diagnostic.
  dynamic_cast<Incomplete&&>(make_incomplete()); // expected-error {{incomplete return type}} expected-error {{an incomplete type}}
}
}
