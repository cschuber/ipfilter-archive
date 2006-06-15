/*
 * Copyright (C) 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <ctype.h>
#include "ipf.h"
#ifdef	IPFILTER_SCAN
# include "netinet/ip_scan.h"
#endif
#include <sys/ioctl.h>
#include <syslog.h>
#ifdef	TEST_LEXER
# define	NO_YACC
union	{
	int		num;
	char		*str;
	struct in_addr	ipa;
	i6addr_t	ip6;
} yylval;
#endif
#include "lexer.h"
#include "y.tab.h"

FILE *yyin;

#define	ishex(c)	(isdigit(c) || ((c) >= 'a' && (c) <= 'f') || \
			 ((c) >= 'A' && (c) <= 'F'))
#define	TOOLONG		-3

extern int	string_start;
extern int	string_end;
extern char	*string_val;
extern int	pos;

char		*yystr = NULL;
int		yytext[YYBUFSIZ+1];
int		yylineNum = 1;
int		yypos = 0;
int		yylast = -1;
int		yyexpectaddr = 0;
int		yybreakondot = 0;
int		yyvarnext = 0;
int		yytokentype = 0;
wordtab_t	*yywordtab = NULL;

static	wordtab_t	*yyfindkey __P((char *));
static	int		yygetc __P((void));
static	void		yyunputc __P((int));
static	int		yyswallow __P((int));
static	char		*yytexttostr __P((int, int));
static	void		yystrtotext __P((char *));


static int yygetc()
{
	int c;

	if (yypos < yylast) {
		c = yytext[yypos++];
		return c;
	}

	if (yypos == YYBUFSIZ)
		return TOOLONG;

	if (pos >= string_start && pos <= string_end) {
		c = string_val[pos - string_start];
		yypos++;
	} else {
		c = fgetc(yyin);
	}
	yytext[yypos++] = c;
	yylast = yypos;
	yytext[yypos] = '\0';

	return c;
}


static void yyunputc(c)
int c;
{
	if (c == '\n')
		yylineNum--;
	yytext[--yypos] = c;
}


static int yyswallow(last)
int last;
{
	int c;

	while (((c = yygetc()) > '\0') && (c != last))
		;

	if (c != EOF)
		yyunputc(c);
	if (c == last)
		return 0;
	return -1;
}


static void yystrtotext(str)
char *str;
{
	int len;
	char *s;

	len = strlen(str);
	if (len > YYBUFSIZ)
		len = YYBUFSIZ;

	for (s = str; *s != '\0' && len > 0; s++, len--)
		yytext[yylast++] = *s;
	yytext[yylast] = '\0';
}


static char *yytexttostr(offset, max)
int offset, max;
{
	char *str;
	int i;

	if ((yytext[offset] == '\'' || yytext[offset] == '"') &&
	    (yytext[offset] == yytext[offset + max - 1])) {
		offset++;
		max--;
	}

	if (max > yylast)
		max = yylast;
	str = malloc(max + 1);
	if (str != NULL) {
		for (i = offset; i < max; i++)
			str[i - offset] = (char)(yytext[i] & 0xff);
		str[i - offset] = '\0';
	}
	return str;
}


int yylex()
{
	int c, n, isbuilding, rval, lnext;
	char *name;

	isbuilding = 0;
	lnext = 0;
	rval = 0;

	if (yystr != NULL) {
		free(yystr);
		yystr = NULL;
	}

nextchar:
	c = yygetc();

	switch (c)
	{
	case '\n' :
		yylineNum++;
	case '\t' :
	case '\r' :
	case ' ' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		if (yylast > yypos) {
			bcopy(yytext + yypos, yytext,
			      sizeof(yytext[0]) * (yylast - yypos + 1));
		}
		yylast -= yypos;
		yypos = 0;
		goto nextchar;

	case '\\' :
		if (lnext == 0) {
			lnext = 1;
			if (yylast == yypos) {
				yylast--;
				yypos--;
			} else
				yypos--;
			goto nextchar;
		}
		break;
	}

	if (lnext == 1) {
		lnext = 0;
		goto nextchar;
	}

	switch (c)
	{
	case '#' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		yyswallow('\n');
		rval = YY_COMMENT;
		goto done;

	case '$' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		n = yygetc();
		if (n == '{') {
			if (yyswallow('}') == -1) {
				rval = -2;
				goto done;
			}
			(void) yygetc();
		} else {
			if (!isalpha(n)) {
				yyunputc(n);
				break;
			}
			do {
				n = yygetc();
			} while (isalpha(n) || isdigit(n) || n == '_');
			yyunputc(n);
		}

		name = yytexttostr(1, yypos);		/* skip $ */

		if (name != NULL) {
			string_val = get_variable(name, NULL, yylineNum);
			free(name);
			if (string_val != NULL) {
				name = yytexttostr(yypos, yylast);
				if (name != NULL) {
					yypos = 0;
					yylast = 0;
					yystrtotext(string_val);
					yystrtotext(name);
					free(string_val);
					free(name);
					goto nextchar;
				}
				free(string_val);
			}
		}
		break;

	case '\'':
	case '"' :
		if (isbuilding == 1) {
			goto done;
		}
		do {
			n = yygetc();
			if (n == EOF || n == TOOLONG) {
				rval = -2;
				goto done;
			}
			if (n == '\n') {
				yyunputc(' ');
				yypos++;
			}
		} while (n != c);
		yyunputc(n);
		break;

	case EOF :
		return 0;
	}

	if (strchr("=,/;{}()@", c) != NULL) {
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		rval = c;
		goto done;
	} else if (c == '.') {
		if (isbuilding == 0) {
			rval = c;
			goto done;
		}
		if (yybreakondot != 0) {
			yyunputc(c);
			goto done;
		}
	}

	switch (c)
	{
	case '-' :
		if (isbuilding == 1)
			break;
		n = yygetc();
		if (n == '>') {
			isbuilding = 1;
			goto done;
		}
		yyunputc(n);
		rval = '-';
		goto done;

	case '!' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		n = yygetc();
		if (n == '=') {
			rval = YY_CMP_NE;
			goto done;
		}
		yyunputc(n);
		rval = '!';
		goto done;

	case '<' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		n = yygetc();
		if (n == '=') {
			rval = YY_CMP_LE;
			goto done;
		}
		if (n == '>') {
			rval = YY_RANGE_OUT;
			goto done;
		}
		yyunputc(n);
		rval = YY_CMP_LT;
		goto done;

	case '>' :
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		n = yygetc();
		if (n == '=') {
			rval = YY_CMP_GE;
			goto done;
		}
		if (n == '<') {
			rval = YY_RANGE_IN;
			goto done;
		}
		yyunputc(n);
		rval = YY_CMP_GT;
		goto done;
	}

	/*
	 * Now for the reason this is here...IPv6 address parsing.
	 * The longest string we can expect is of this form:
	 * 0000:0000:0000:0000:0000:0000:000.000.000.000
	 * not:
	 * 0000:0000:0000:0000:0000:0000:0000:0000
	 */
#ifdef	USE_INET6
	if (yyexpectaddr == 1 && isbuilding == 0 && (ishex(c) || c == ':')) {
		char ipv6buf[45 + 1], *s, oc;
		int start;

		start = yypos;
		s = ipv6buf;
		oc = c;

		/*
		 * Perhaps we should implement stricter controls on what we
		 * swallow up here, but surely it would just be duplicating
		 * the code in inet_pton() anyway.
		 */
		do {
			*s++ = c;
			c = yygetc();
		} while ((ishex(c) || c == ':' || c == '.') &&
			 (s - ipv6buf < 46));
		yyunputc(c);
		*s = '\0';

		if (inet_pton(AF_INET6, ipv6buf, &yylval.ip6) == 1) {
			rval = YY_IPV6;
			yyexpectaddr = 0;
			goto done;
		}
		yypos = start;
		c = oc;
	}
#endif

	if (c == ':') {
		if (isbuilding == 1) {
			yyunputc(c);
			goto done;
		}
		rval = ':';
		goto done;
	}

	if (isbuilding == 0 && c == '0') {
		n = yygetc();
		if (n == 'x') {
			do {
				n = yygetc();
			} while (ishex(n));
			yyunputc(n);
			rval = YY_HEX;
			goto done;
		}
		yyunputc(n);
	}

	/*
	 * No negative numbers with leading - sign..
	 */
	if (isbuilding == 0 && isdigit(c)) {
		do {
			n = yygetc();
		} while (isdigit(n));
		yyunputc(n);
		rval = YY_NUMBER;
		goto done;
	}

	isbuilding = 1;
	goto nextchar;

done:
	yystr = yytexttostr(0, yypos);

	if (isbuilding == 1) {
		wordtab_t *w;

		w = NULL;
		isbuilding = 0;

		if (yyvarnext == 0)
			w = yyfindkey(yystr);
		else
			yyvarnext = 0;
		if (w != NULL)
			rval = w->w_value;
		else
			rval = YY_STR;
	}
	yytokentype = rval;

	switch (rval)
	{
	case YY_NUMBER :
		yylval.num = atoi(yystr);
		break;

	case YY_HEX :
		sscanf(yystr, "0x%x", (u_int *)&yylval.num);
		break;

	case YY_STR :
		yylval.str = strdup(yystr);
		break;

	default :
		break;
	}

	if (yylast > 0) {
		bcopy(yytext + yypos, yytext,
		      sizeof(yytext[0]) * (yylast - yypos + 1));
		yylast -= yypos;
		yypos = 0;
	}

	return rval;
}


static wordtab_t *yyfindkey(key)
char *key;
{
	wordtab_t *w;

	if (yywordtab == NULL)
		return NULL;

	for (w = yywordtab; w->w_word != 0; w++)
		if (strcasecmp(key, w->w_word) == 0)
			return w;
	return NULL;
}


char *yykeytostr(num)
int num;
{
	wordtab_t *w;

	if (yywordtab == NULL)
		return "<unknown>";

	for (w = yywordtab; w->w_word; w++)
		if (w->w_value == num)
			return w->w_word;
	return "<unknown>";
}


wordtab_t *yysettab(words)
wordtab_t *words;
{
	wordtab_t *save;

	save = yywordtab;
	yywordtab = words;
	return save;
}


void yyerror(msg)
char *msg;
{
	char *txt, letter[2];
	int freetxt = 0;

	if (yytokentype < 256) {
		letter[0] = yytokentype;
		letter[1] = '\0';
		txt =  letter;
	} else if (yytokentype == YY_STR || yytokentype == YY_HEX ||
		   yytokentype == YY_NUMBER) {
		if (yystr == NULL) {
			txt = yytexttostr(yypos, YYBUFSIZ);
			freetxt = 1;
		} else
			txt = yystr;
	} else {
		txt = yykeytostr(yytokentype);
	}
	fprintf(stderr, "%s error at \"%s\", line %d\n", msg, txt, yylineNum);
	if (freetxt == 1)
		free(txt);
	exit(1);
}


#ifdef	TEST_LEXER
int main(argc, argv)
int argc;
char *argv[];
{
	int n;

	yyin = stdin;

	while ((n = yylex()) != 0)
		printf("%d.n = %d [%s] %d %d\n",
			yylineNum, n, yystr, yypos, yylast);
}
#endif