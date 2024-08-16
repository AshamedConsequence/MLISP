#include "parsing.h"
#include "mpc.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>
#include <editline/readline.h>
#include <string.h>

#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    token_delete(args);                                                        \
    return token_err(err);                                                     \
  }

Token *token_num(long x) {
  Token *v = malloc(sizeof(Token));
  v->type = NUM;
  v->num = x;
  return v;
}

Token *token_float(double x) {
  Token *v = malloc(sizeof(Token));
  v->type = FLOAT;
  v->frac = x;
  return v;
}

Token *token_err(char *message) {
  Token *v = malloc(sizeof(Token));
  v->type = ERR;
  v->err = malloc(strlen(message) + 1);
  strcpy(v->err, message);
  return v;
}

Token *token_sym(char *symbol) {
  Token *v = malloc(sizeof(Token));
  v->type = SYM;
  v->sym = malloc(strlen(symbol) + 1);
  strcpy(v->sym, symbol);
  return v;
}

Token *token_sexpr(void) {
  Token *v = malloc(sizeof(Token));
  v->type = SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}
Token *token_qexpr(void) {
  Token *v = malloc(sizeof(Token));
  v->type = QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void token_delete(Token *v) {
  switch (v->type) {
  case NUM:
  case FLOAT:
    break;
  case ERR:
    free(v->err);
    break;
  case SYM:
    free(v->sym);
    break;
  case SEXPR:
  case QEXPR:
    for (int i = 0; i < v->count; i++) {
      token_delete(v->cell[i]);
    }
    free(v->cell);
    break;
  }

  free(v);
}

Token *token_read_num(mpc_ast_t *tree) {
  errno = 0;
  long x = strtol(tree->contents, NULL, 10);
  return errno != ERANGE ? token_num(x) : token_err("Invalid Number");
}

Token *token_read_float(mpc_ast_t *tree) {
  errno = 0;
  double x = strtod(tree->contents, NULL);
  return errno != ERANGE ? token_float(x) : token_err("Invalid Number");
}

Token *token_add(Token *v, Token *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(Token *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

Token *token_read(mpc_ast_t *tree) {
  if (strstr(tree->tag, "number")) {
    if (strstr(tree->contents, ".")) {
      return token_read_float(tree);
    }
    return token_read_num(tree);
  }
  if (strstr(tree->tag, "symbol")) {
    return token_sym(tree->contents);
  }

  Token *x = NULL;

  if (strcmp(tree->tag, ">") == 0) {
    x = token_sexpr();
  }
  if (strstr(tree->tag, "sexpr")) {
    x = token_sexpr();
  }
  if (strstr(tree->tag, "qexpr")) {
    x = token_qexpr();
  }

  for (int i = 0; i < tree->children_num; i++) {
    if (strcmp(tree->children[i]->contents, "(") == 0) {
      continue;
    }
    if (strcmp(tree->children[i]->contents, ")") == 0) {
      continue;
    }
    if (strcmp(tree->children[i]->tag, "regex") == 0) {
      continue;
    }
    x = token_add(x, token_read(tree->children[i]));
  }

  return x;
}

void token_expr_print(Token *v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    token_print(v->cell[i]);
    // Make sure to not add trailing space to last element
    if (i != v->count - 1) {
      putchar(' ');
    }
  }

  putchar(close);
}

void token_print(Token *v) {
  switch (v->type) {
  case NUM:
    printf("%li", v->num);
    break;
  case FLOAT:
    printf("%g", v->frac);
    break;
  case ERR:
    printf("Error: %s", v->err);
    break;
  case SYM:
    printf("%s", v->sym);
    break;
  case SEXPR:
    token_expr_print(v, '(', ')');
    break;
  case QEXPR:
    token_expr_print(v, '{', '}');
    break;
  }
}

void token_println(Token *v) {
  token_print(v);
  putchar('\n');
}

Token *token_pop(Token *v, int i) {
  Token *x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i + 1], sizeof(Token *) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(Token *) * v->count);
  return x;
}

Token *token_take(Token *v, int i) {
  Token *x = token_pop(v, i);
  token_delete(v);
  return x;
}

Token *token_join(Token *x, Token *y) {
  while (y->count != 0) {
    x = token_add(x, token_pop(y, 0));
  }
  token_delete(y);
  return x;
}

Token *builtin_head(Token *arg) {
  LASSERT(arg, arg->count == 1, "Function 'head' passed too many arguments");
  LASSERT(arg, arg->cell[0]->type == QEXPR,
          "Function 'head' passed incorrect types");
  LASSERT(arg, arg->cell[0]->count != 0, "Function 'head' passed {}");
  Token *v = token_take(arg, 0);
  while (v->count > 1) {
    token_delete(token_pop(v, 1));
  }
  return v;
}

Token *builtin_tail(Token *arg) {
  LASSERT(arg, arg->count == 1, "Function 'tail' passed too many arguments");
  LASSERT(arg, arg->cell[0]->type == QEXPR,
          "Function 'tail' passed incorrect types");
  LASSERT(arg, arg->cell[0]->count != 0, "Function 'tail' passed {}");
  Token *v = token_take(arg, 0);
  token_delete(token_pop(v, 0));
  return v;
}

Token *builtin_list(Token *arg) {
  arg->type = QEXPR;
  return arg;
}

Token *builtin_eval(Token *arg) {
  LASSERT(arg, arg->count == 1, "Function 'eval' passed too many arguments");
  LASSERT(arg, arg->cell[0]->type == QEXPR,
          "Function 'eval' passed incorrect types");

  Token *x = token_take(arg, 0);
  x->type = SEXPR;
  return token_eval(x);
}

Token *builtin_join(Token *arg) {
  for (int i = 0; i < arg->count; i++) {
    LASSERT(arg, arg->cell[i]->type == QEXPR,
            "Function 'join' passed incorrect types!");
  }

  Token *x = token_pop(arg, 0);
  while (arg->count != 0) {
    x = token_join(x, token_pop(arg, 0));
  }
  token_delete(arg);
  return x;
}

Token *builtin_op(Token *arg, char *op) {
  for (int i = 0; i < arg->count; i++) {
    if (arg->cell[i]->type != NUM && arg->cell[i]->type != FLOAT) {
      token_delete(arg);
      return token_err("Cannot operate on a non-number");
    }
  }
  Token *x = token_pop(arg, 0);

  if (x->type == NUM) {
    if ((strcmp(op, "-") == 0) && arg->count == 0) {
      x->num = -x->num;
    }

    while (arg->count > 0) {
      Token *y = token_pop(arg, 0);
      if (strcmp(op, "+") == 0) {
        x->num += y->num;
      }
      if (strcmp(op, "%") == 0) {
        if (y->num == 0) {
          token_delete(x);
          token_delete(y);
          x = token_err("Modulus by Zero!");
          break;
        }
        x->num = x->num % y->num;
      }
      if (strcmp(op, "-") == 0) {
        x->num -= y->num;
      }
      if (strcmp(op, "*") == 0) {
        x->num *= y->num;
      }
      if (strcmp(op, "/") == 0) {
        if (y->num == 0) {
          token_delete(x);
          token_delete(y);
          x = token_err("Division by Zero!");
          break;
        }
        x->num /= y->num;
      }
      token_delete(y);
    }
  } else {
    if ((strcmp(op, "-") == 0) && arg->count == 0) {
      x->frac = -x->frac;
    }

    while (arg->count > 0) {
      Token *y = token_pop(arg, 0);
      if (strcmp(op, "+") == 0) {
        x->frac += y->frac;
      }
      if (strcmp(op, "%") == 0) {
        if (y->frac == 0) {
          token_delete(x);
          token_delete(y);
          x = token_err("Modulus by Zero!");
          break;
        }
        x->frac = fmod(x->frac, y->frac);
      }
      if (strcmp(op, "-") == 0) {
        x->frac -= y->frac;
      }
      if (strcmp(op, "*") == 0) {
        x->frac *= y->frac;
      }
      if (strcmp(op, "/") == 0) {
        if (y->frac == 0) {
          token_delete(x);
          token_delete(y);
          x = token_err("Division by Zero!");
          break;
        }
        x->frac /= y->frac;
      }
      token_delete(y);
    }
  }
  token_delete(arg);
  return x;
}

Token *token_eval_sexpr(Token *v) {
  int float_flag = 0;
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = token_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == ERR) {
      return token_take(v, i);
    }
    if (v->cell[i]->type == FLOAT && float_flag == 0) {
      for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == NUM) {
          v->cell[i]->type = FLOAT;
          v->cell[i]->frac = (double)v->cell[i]->num;
        }
      }
      float_flag = 1;
    }
  }

  if (v->count == 0) {
    return v;
  }
  if (v->count == 1) {
    return token_take(v, 0);
  }

  Token *first = token_pop(v, 0);
  if (first->type != SYM) {
    token_delete(v);
    token_delete(first);
    return token_err("S-expression does not begin with Symbol");
  }

  Token *token = builtin(v, first->sym);
  token_delete(first);
  return token;
}

Token *token_eval(Token *v) {
  if (v->type == SEXPR) {
    return token_eval_sexpr(v);
  }
  return v;
}

Token *builtin(Token *arg, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(arg);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(arg);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(arg);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(arg);
  }
  if (strcmp("eval", func) == 0) {
    return builtin_eval(arg);
  }
  if (strstr("+-*/", func)) {
    return builtin_op(arg, func);
  }
  token_delete(arg);
  return token_err("Unknown function!");
}
int main(int argc, char **argv) {
  /* Creating the parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Adding definitions to these parsers */
  mpca_lang(MPCA_LANG_DEFAULT, "                                             \
    number: /-?[0-9]+(\\.[0-9]+)?/;                       \
    symbol: '+' | '-' | '*' | '/' | '%' | '^'             \
    | \"min\" | \"max\" | \"list\"                                  \
    | \"head\" |  \"tail\" | \"join\" | \"eval\";         \
    sexpr: '(' <expr>* ')';                               \
    qexpr: '{' <expr>* '}';                               \
    expr: <number> | <symbol> | <sexpr> | <qexpr> ;       \
    lispy: /^/ <expr>* /$/ ;                              \
      ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  puts("Maxime's Lisp Version 0.0.0.0.8");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char *input = readline("mlisp> ");
    add_history("input");

    mpc_result_t result;
    if (mpc_parse("<stdin>", input, Lispy, &result)) {
      /* On success parse the AST */
      Token *answer = token_eval(token_read(result.output));
      /* Result *answer = result_read(result.output); */
      token_println(answer);
      token_delete(answer);
    } else {
      /* Otherwise print the error */
      mpc_err_print(result.error);
      mpc_err_delete(result.error);
    }
    free(input);
  }
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
