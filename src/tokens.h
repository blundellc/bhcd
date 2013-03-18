#ifndef	TOKENS_H
#define	TOKENS_H

#include <glib.h>

struct Tokens_t;
typedef struct Tokens_t Tokens;

Tokens * tokens_open(const gchar *fname);
void tokens_fail(Tokens * toks, const gchar *fmt, ...);
void tokens_close(Tokens * toks);

gboolean tokens_has_next(Tokens * toks);
gchar * tokens_next(Tokens * toks);

void tokens_expect(Tokens * toks, const gchar *token);
void tokens_expect_end(Tokens * toks);
gboolean tokens_peek_test(Tokens * toks, const gchar *token);

#endif /*TOKENS_H*/
