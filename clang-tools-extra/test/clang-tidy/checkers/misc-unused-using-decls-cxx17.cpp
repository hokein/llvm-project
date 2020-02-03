// RUN: %check_clang_tidy -std=c++17-or-later %s misc-unused-using-decls %t -- -- -fno-delayed-template-parsing -isystem %S/Inputs/

namespace ns {
template <typename K, typename V>
class KV {
public:
  KV(K, V);
};
}

using ns::KV;

void f() {
  KV(1, 2);
}
