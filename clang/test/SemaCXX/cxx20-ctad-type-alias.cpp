// RUN: %clang_cc1 -fsyntax-only -Wno-c++11-narrowing -Wno-literal-conversion -std=c++20 -verify %s
// expected-no-diagnostics

template<typename T>
struct Foo {
  T t;
};

template<typename U>
using Bar = Foo<U>;

void test1() {
  Bar s = {1};
}

template<typename X, typename Y>
struct XYpair {
  X x;
  Y y;
};
// A tricky explicit deduction guide that swapping X and Y.
template<typename X, typename Y>
XYpair(X, Y) -> XYpair<Y, X>;
template<typename U, typename V>
using AliasXYpair = XYpair<U, V>;

void test2() {
  AliasXYpair xy = {1.1, 2}; // XYpair<int, double>

  static_assert(__is_same(decltype(xy.x), int));
  static_assert(__is_same(decltype(xy.y), double));
}
