// RUN: %clang_cc1 -ffreestanding -fsyntax-only -target-feature +ssse3 -target-feature +mmx -verify -triple x86_64-pc-linux-gnu -frecovery-ast %s

#include <tmmintrin.h>

// FIXME: this can be moved to x86-builin-palignr-recovery.c when recovery-ast is enabled for C by default.
__m64 test1(__m64 a, __m64 b, int c) {
   // verify no diagnostic "operand of type '<dependent type>' where arithmetic or pointer type is required".
   return _mm_alignr_pi8(a, b, c); // expected-error {{argument to '__builtin_ia32_palignr' must be a constant integer}}
}
