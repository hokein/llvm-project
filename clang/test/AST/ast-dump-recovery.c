// RUN: not %clang_cc1 -triple x86_64-unknown-unknown -frecovery-ast -ast-dump %s | FileCheck -strict-whitespace %s

int some_func(int);

// CHECK:     VarDecl {{.*}} unmatch_arg_call 'int' cinit
// CHECK-NEXT: `-RecoveryExpr {{.*}} contains-errors
// CHECK-NEXT:   `-DeclRefExpr {{.*}} 'some_func'
int unmatch_arg_call = some_func();

const int a = 1; 

// CHECK:     VarDecl {{.*}} postfix_inc
// CHECK-NEXT: `-RecoveryExpr {{.*}} contains-errors
// CHECK-NEXT:   `-DeclRefExpr {{.*}} 'a'
int postfix_inc = a++;

// CHECK:     VarDecl {{.*}} prefix_inc
// CHECK-NEXT:`-RecoveryExpr {{.*}} contains-errors
// CHECK-NEXT:  `-DeclRefExpr {{.*}} 'a'
int prefix_inc = ++a;

// CHECK:     VarDecl {{.*}} unary_address
// CHECK-NEXT:`-RecoveryExpr {{.*}} contains-errors
// CHECK-NEXT:  `-ParenExpr {{.*}}
// CHECK-NEXT:    `-BinaryOperator {{.*}} '+'
// CHECK-NEXT:      |-ImplicitCastExpr
// CHECK-NEXT:      | `-DeclRefExpr {{.*}} 'a'
// CHECK-NEXT:      `-IntegerLiteral {{.*}} 'int'
int unary_address = &(a + 1);

// CHECK:     VarDecl {{.*}} unary_bitinverse
// CHECK-NEXT:`-RecoveryExpr {{.*}} contains-errors
// CHECK-NEXT:  `-ParenExpr {{.*}}
// CHECK-NEXT:    `-BinaryOperator {{.*}} '+'
// CHECK-NEXT:      |-ImplicitCastExpr
// CHECK-NEXT:      | `-ImplicitCastExpr
// CHECK-NEXT:      |   `-DeclRefExpr {{.*}} 'a'
// CHECK-NEXT:      `-FloatingLiteral {{.*}} 'double'
int unary_bitinverse = ~(a + 0.0);

// CHECK:       VarDecl {{.*}} ternary 'int' cinit
// CHECK-NEXT:  `-RecoveryExpr {{.*}}
// CHECK-NEXT:    |-DeclRefExpr {{.*}} 'a'
// CHECK-NEXT:    |-TypoExpr {{.*}}
// CHECK-NEXT:    `-DeclRefExpr {{.*}} 'a'
// FIXME: The TypoExpr should never be print, and should be downgraded to
// RecoveryExpr -- typo correction is performed too early in C-only codepath,
// which makes no correction when clang finishes the full expr (Sema::Sema::ActOnFinishFullExpr).
// this will be fixed when we support dependent mechanism and delayed typo correction for C.
int ternary = a ? undef : a;
