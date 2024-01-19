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

namespace test3 {
template<typename T, class>
struct container {
  // test with default arguments.
  container(T a , T b = T());
};

template<class T> using vector = container<T, int>;
vector v(0, 0);
} // namespace test3