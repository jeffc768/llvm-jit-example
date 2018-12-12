%{
#include "AST.h"
#include <string>

int yylex();
void yyerror(const char *s);

// Output of parser.
AST *yyparsetree = nullptr;

%}

// All possible types of values associated with terminals and non-terminals.
%union {
    Number      *number;
    Name        *name;
    Expr        *expr;
    Function    *func;
    AST         *ast;
}

// Tokens in addition to the usual single-character suspects.
%token          kw_fun
%token          kw_quit
%token          op_neg    // distinguishes unary from binary '-'
%token          EOL       // because bison doesn't support '\n'
%token <name>   t_name
%token <number> t_number

// Define operator precedence (from lowest to highest) and associativity.
%right <lexeme> '='
%left  <lexeme> '?' ':'
%left  <lexeme> '|'
%left  <lexeme> '^'
%left  <lexeme> '&'
%left  <lexeme> op_eq op_ne
%left  <lexeme> '<' '>' op_le op_ge
%left  <lexeme> '+' '-'
%left  <lexeme> '*' '/' '%'
%right <lexeme> unary_precedence
%left  <lexeme> '('

// Define type of values produced by non-terminals.
%type <expr>    Expr;
%type <func>    Function;
%type <ast>     Line;

%%

Line:
      Function EOL
        { yyparsetree = $1; }
    | Expr EOL
        { yyparsetree = $1; }
    | kw_quit EOL
        { exit(0); }
    ;

Function:
      kw_fun t_name '(' t_name ')' '=' Expr
        { $$ = new Function(kw_fun, $2, $4, $7); }
    ;

Expr:
      t_name
        { $$ = $1; }
    | t_number
        { $$ = $1; }
    | '(' Expr ')'
        { $$ = $2; }
    | t_name '(' Expr ')'
        { $$ = new Operator('(', $1, $3); }
    | t_name '=' Expr
        { $$ = new Operator('=', $1, $3); }
    | Expr '+' Expr
        { $$ = new Operator('+', $1, $3); }
    | Expr '-' Expr
        { $$ = new Operator('-', $1, $3); }
    | Expr '*' Expr
        { $$ = new Operator('*', $1, $3); }
    | Expr '/' Expr
        { $$ = new Operator('/', $1, $3); }
    | Expr '%' Expr
        { $$ = new Operator('%', $1, $3); }
    | Expr '<' Expr
        { $$ = new Operator('<', $1, $3); }
    | Expr '>' Expr
        { $$ = new Operator('>', $1, $3); }
    | Expr op_eq Expr
        { $$ = new Operator(op_eq, $1, $3); }
    | Expr op_ne Expr
        { $$ = new Operator(op_ne, $1, $3); }
    | Expr op_le Expr
        { $$ = new Operator(op_le, $1, $3); }
    | Expr op_ge Expr
        { $$ = new Operator(op_ge, $1, $3); }
    | Expr '|' Expr
        { $$ = new Operator('|', $1, $3); }
    | Expr '&' Expr
        { $$ = new Operator('&', $1, $3); }
    | Expr '^' Expr
        { $$ = new Operator('^', $1, $3); }
    | '-' Expr %prec unary_precedence
        { $$ = new Operator(op_neg, $2); }
    | '!' Expr %prec unary_precedence
        { $$ = new Operator('!', $2); }
    | '~' Expr %prec unary_precedence
        { $$ = new Operator('~', $2); }
    | Expr '?' Expr ':' Expr
        { $$ = new Operator('?', $1, $3, $5); }
    ;

%%

// Function required by bison generated code.
void yyerror(const char *s) {
    fprintf(stderr, "%s\n", s);
}
