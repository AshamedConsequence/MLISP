#ifndef PARSING_H_
#define PARSING_H_
#include "mpc.h"
typedef struct Token {
  int type;
  union {
    long num;
    double frac;
  };

  char *err;
  char *sym;

  int count;
  struct Token **cell;
} Token;

enum TYPE { NUM, FLOAT, ERR, SYM, SEXPR, QEXPR };

enum ERR_TYPE { ERR_DIV_ZERO, ERR_BAD_OP, ERR_BAD_NUM };

Token *token_num(long x);
Token *token_float(double x);
Token *token_err(char *message);
Token *token_sym(char *symbol);
Token *token_sexpr(void);
Token *token_qexpr(void);
void token_delete(Token *v);
Token *token_read_num(mpc_ast_t *tree);
Token *token_read_float(mpc_ast_t *tree);
Token *token_add(Token *v, Token *x);
Token *token_join(Token *x, Token *y);
Token *token_read(mpc_ast_t *tree);
void token_print(Token *v);
void token_expr_print(Token *v, char open, char close);
void token_println(Token *v);
Token *token_eval(Token *v);
Token *token_pop(Token *v, int i);
Token *token_take(Token *v, int i);
Token *builtin_op(Token *arg, char *op);
Token *builtin_head(Token *arg);
Token *builtin_tail(Token *arg);
Token *builtin_join(Token *arg);
Token *builtin_eval(Token *arg);
Token *builtin_list(Token *arg);
Token *builtin(Token *arg, char *func);
Token *token_eval_sexpr(Token *v);
#endif // PARSING_H_
