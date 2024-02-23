/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* Lexer for Baby Assembler. */

%{
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "arch.h"
#include "asm.h"
#include "strtab.h"

/* yacc-generated definitions */
#include "asm-parse.h"

/* suppress -Werror=unused-function */
static int input() __attribute__((unused));
static void yyunput() __attribute__((unused));

static void update_loc(YYLTYPE *yylloc, YYSTYPE *yylval) {
  char *ptr;

  if (yylloc->last_char_was_newline) {
    yylloc->end.line++;
    yylloc->end.col = 1;
  } else {
    yylloc->end.col++;
  }
  yylloc->end.offset++;
  yylloc->start = yylloc->end;
  yylloc->last_char_was_newline = false;

  for (ptr = yytext; *ptr != '\0'; ptr++) {
    if (*ptr == '\n') {
      if (ptr[1] != '\0') {
        yylloc->end.line++;
        yylloc->end.col = 1;
      } else {
        yylloc->last_char_was_newline = true;
      }
    } else {
      if (ptr[1] != '\0') {
        yylloc->end.col++;
      }
    }
    yylloc->end.offset++;
  }
  yylloc->end.offset--;
}

static str_idx_t strput(const char *text) {
  return strtab_put(strtab_src, text);
}

#define YY_USER_ACTION update_loc(yylloc, yylval);
%}

%option noyywrap
%option bison-locations

%x comment

%%

\\[ \t]*\n              { /* continuation */ }
[ \t]+                  { }
\n+                     { return EOL; }
-?0[xX][[:xdigit:]]+    { yylval->HEX = strdup(yytext); return HEX; }
-?0[0-7]+               { yylval->OCTAL = strdup(yytext); return OCTAL; }
-?[[:digit:]]+          { yylval->DECIMAL = strdup(yytext); return DECIMAL; }
-?0[bB][01]+            { yylval->BINARY = strdup(yytext); return BINARY; }

(?i:MACRO)              { return MACRO; }
(?i:ENDM)               { return ENDM; }

[_.$a-zA-Z][_.$a-zA-Z0-9]*  { yylval->NAME = strput(yytext); return NAME; }
:                       { return COLON; }
,                       { return COMMA; }
-                       { return MINUS; }
\+                      { return PLUS; }

--                      { BEGIN(comment); }
;                       { BEGIN(comment); }
<comment>[^\n]*$        { BEGIN(INITIAL); /* return COMMENT; */ }

%%

