#include <memory>
#include <string>

#pragma once

// Various nodes of the Abstract Syntax Tree constructed by the parser.

struct AST {
    int                     lexeme;

    AST(int l) : lexeme(l) { }
    virtual ~AST() { }
};

struct Expr: public AST {
    Expr(int l) : AST(l) { }
};

struct Number: public Expr {
    int                     value;

    Number(int l, int v) : Expr(l), value(v) { }
};

struct Name: public Expr {
    std::string             value;

    Name(int l, const char *v) : Expr(l), value(v) { }
};

struct Operator: public Expr {
    std::unique_ptr<Expr>   arg1;
    std::unique_ptr<Expr>   arg2;
    std::unique_ptr<Expr>   arg3;

    Operator(int l, Expr *a1, Expr *a2 = nullptr, Expr *a3 = nullptr)
          : Expr(l), arg1(a1), arg2(a2), arg3(a3) { }
};

struct Function: public AST {
    std::string             name;
    std::string             arg;
    std::unique_ptr<Expr>   body;

    Function(int l, Name *n, Name *a, Expr *b)
          : AST(l), name(n->value), arg(a->value), body(b) {
        delete n;
        delete a;
    }
};
