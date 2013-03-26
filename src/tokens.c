#include <stdarg.h>
#include <string.h>
#include <glib/gprintf.h>
#include "tokens.h"

struct Tokens_t {
	gboolean debug;
	GIOChannel * io;
	gchar * fname;
	guint lineno;

	gchar *linebuf;
	gchar *cur;
	gchar *next;
};

void tokens_advance(Tokens * toks);
void tokens_advance_line(Tokens *toks);
void tokens_advance_try_next(Tokens *toks);


Tokens * tokens_open(const gchar *fname) {
	Tokens * toks;
	GError *error;

	toks = g_new(Tokens, 1);
	toks->debug = FALSE;
	toks->fname = g_strdup(fname);
	toks->next = NULL;
	toks->cur = NULL;
	toks->linebuf = NULL;
	toks->lineno = 0;
	error = NULL;
	toks->io = g_io_channel_new_file(fname, "r", &error);
	if (toks->io == NULL) {
		tokens_fail(toks, "unable to open file: %s", error->message);
	}
	tokens_advance(toks);
	return toks;
}

void tokens_fail(Tokens * toks, const gchar *fmt, ...) {
	va_list ap;
	gchar *msg;

	va_start(ap, fmt);
	g_vasprintf(&msg, fmt, ap);
	va_end(ap);

	g_error("%s:%d: parse error: %s", toks->fname, toks->lineno, msg);
}

void tokens_close(Tokens * toks) {
	if (toks->io != NULL) {
		g_io_channel_unref(toks->io);
	}
	g_free(toks->linebuf);
	g_free(toks->fname);
	g_free(toks->next);
	g_free(toks);
}

void tokens_advance(Tokens * toks) {
	toks->next = NULL;
	while (toks->next == NULL || strlen(toks->next) == 0) {
		/* end of line */
		if (toks->cur == NULL) {
			tokens_advance_line(toks);
		}
		/* end of file */
		if (toks->linebuf == NULL) {
			break;
		}
		tokens_advance_try_next(toks);
	}
}

void tokens_advance_line(Tokens *toks) {
	GError *error;

	g_free(toks->linebuf);
	error = NULL;
	g_io_channel_read_line(toks->io, &toks->linebuf, NULL, NULL, &error);
	if (error != NULL) {
		tokens_fail(toks, "read line: %s", error->message);
	}
	toks->lineno++;
	if (toks->debug) {
		g_print("line %d: %s", toks->lineno, toks->linebuf);
	}
	toks->cur = toks->linebuf;
	toks->next = NULL;
}

void tokens_advance_try_next(Tokens *toks) {
	g_assert(toks->cur != NULL);

	toks->next = toks->cur;
	while (*toks->next != '\0' && g_ascii_isspace(*toks->next)) {
		toks->next++;
	}

	toks->cur = toks->next;
	while (*toks->cur != '\0' && !g_ascii_isspace(*toks->cur)) {
		toks->cur++;
	}
	if (*toks->cur == '\0') {
		/* end of line */
		toks->cur = NULL;
	} else {
		*toks->cur = '\0';
		toks->cur++;
	}
}

gboolean tokens_has_next(Tokens * toks) {
	return toks->next != NULL;
}

gchar * tokens_next(Tokens * toks) {
	gchar *next;

	if (!tokens_has_next(toks)) {
		tokens_fail(toks, "expected a token; none found");
	}
	next = g_strdup(toks->next);
	tokens_advance(toks);
	return next;
}

gchar * tokens_next_quoted(Tokens * toks) {
	gchar *next;
	gchar *rest;
	gchar *tmp;
	size_t len;

	next = tokens_next(toks);
	len = strlen(next);
	while (next[0] == '"' &&
		(len == 1 || next[len-1] != '"')) {
		rest = tokens_next(toks);
		tmp = next;
		next = g_strconcat(next, " ", rest, NULL);
		g_free(tmp);
		g_free(rest);
		len = strlen(next);
	}
	return next;
}

gdouble tokens_next_double(Tokens * toks) {
	gdouble next;
	gchar *endp;

	if (!tokens_has_next(toks)) {
		tokens_fail(toks, "expected a token; none found");
	}

	next = g_ascii_strtod(toks->next, &endp);
	if (*endp != '\0') {
		tokens_fail(toks, "expected a double; found %s", next);
	}
	tokens_advance(toks);
	return next;
}


gint64 tokens_next_int(Tokens * toks) {
	gint64 next;
	gchar *endp;

	if (!tokens_has_next(toks)) {
		tokens_fail(toks, "expected a token; none found");
	}
	next = g_ascii_strtoll(toks->next, &endp, 0);
	if (*endp != '\0') {
		tokens_fail(toks, "expected a integer; found %s", next);
	}
	tokens_advance(toks);
	return next;
}


void tokens_expect(Tokens * toks, const gchar *token) {
	if (!tokens_has_next(toks)) {
		tokens_fail(toks, "expecting `%s'; nothing found.", token);
	}
	if (strcmp(toks->next, token) != 0) {
		tokens_fail(toks, "expecting `%s'; found `%s'.", token, toks->next);
	}
	tokens_advance(toks);
}

void tokens_expect_end(Tokens * toks) {
	if (tokens_has_next(toks)) {
		tokens_fail(toks, "expecting end; found `%s'.", toks->next);
	}
}

gboolean tokens_peek_test(Tokens * toks, const gchar *token) {
	if (!tokens_has_next(toks)) {
		return FALSE;
	}
	return strcmp(toks->next, token) == 0;
}

