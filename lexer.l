%option 8bit
%option never-interactive
%option noyywrap
%option case-insensitive
%option ecs
%option noinput
%option nounput

%{
#include "AST.h"
#include "parser.h"
%}

%%

">="      return op_ge;
"<="      return op_le;
"=="      return op_eq;
"!="      return op_ne;

[0-9]+    yylval.number = new Number(t_number, atoi(yytext)); return t_number;

fun       return kw_fun;
quit      return kw_quit;

[a-z][a-z0-9_]*   yylval.name = new Name(t_name, yytext); return t_name;

" "       YY_BREAK;
\n        return EOL;
.         return yytext[0];

%%

// Called from main() for each line of input from stdin.
void setLexerInput(const std::string &line) {
    yy_scan_bytes(line.data(), line.size());
}

// Called after parsing is done to free up memory.
void clearLexerInput() {
    yy_delete_buffer(YY_CURRENT_BUFFER);
}
