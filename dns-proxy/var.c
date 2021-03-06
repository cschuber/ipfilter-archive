/*
 * Copyright (c) 2006
 *      Darren Reed.  All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef	struct	variable	{
	struct	variable	*v_next;
	char	*v_name;
	char	*v_value;
} variable_t;

static	variable_t	*vtop = NULL;

static variable_t *find_var __P((char *));
static char *expand_string __P((char *, int));


static variable_t *find_var(name)
char *name;
{
	variable_t *v;

	for (v = vtop; v != NULL; v = v->v_next)
		if (!strcmp(name, v->v_name))
			return v;
	return NULL;
}


char *
get_variable(char *string, char **after, int line)
{
	char *s, *t, *value, temp;
	variable_t *v;
	int c;

	s = string;
	c = *s;

	if (c == '{') {
		s++;
		for (t = s; *t != '\0'; t++)
			if (*t == '}')
				break;
		if (*t == '\0') {
			fprintf(stderr, "%d: { without }\n", line);
			return NULL;
		}
	} else if (isalpha(c)) {
		for (t = s + 1; *t != '\0'; t++) {
			c = *t;
			if (!isalpha(c) && !isdigit(c) && (*t != '_'))
				break;
		}
	} else {
		fprintf(stderr, "%d: variables cannot start with '%c'\n",
			line, *s);
		return NULL;
	}

	if (after != NULL)
		*after = t;
	temp = *t;
	*t = '\0';
	v = find_var(s);
	*t = temp;
	if (v == NULL) {
		fprintf(stderr, "%d: unknown variable '%s'\n", line, s);
		return NULL;
	}

	s = strdup(v->v_value);
	value = expand_string(s, line);
	if (value != s)
		free(s);
	return value;
}


static char *expand_string(oldstring, line)
char *oldstring;
int line;
{
	char c, *s, *p1, *p2, *p3, *newstring, *value;
	int len;

	p3 = NULL;
	newstring = oldstring;

	for (s = oldstring; *s != '\0'; s++)
		if (*s == '$') {
			*s = '\0';
			s++;

			switch (*s)
			{
			case '$' :
				bcopy(s, s - 1, strlen(s));
				break;
			default :
				c = *s;
				if (c == '\0')
					return newstring;

				value = get_variable(s, &p3, line);
				if (value == NULL)
					return NULL;

				p2 = expand_string(value, line);
				if (p2 == NULL)
					return NULL;

				len = strlen(newstring) + strlen(p2);
				if (p3 != NULL) {
					if (c == '{' && *p3 == '}')
						p3++;
					len += strlen(p3);
				}
				p1 = malloc(len + 1);
				if (p1 == NULL)
					return NULL;

				*(s - 1) = '\0';
				strcpy(p1, newstring);
				strcat(p1, p2);
				if (p3 != NULL)
					strcat(p1, p3);

				s = p1 + len - strlen(p3) - 1;
				if (newstring != oldstring)
					free(newstring);
				newstring = p1;
				break;
			}
		}
	return newstring;
}


void
set_variable(char *name, char *value)
{
	variable_t *v;
	int len;

	if (name == NULL || value == NULL || *name == '\0')
		return;

	v = find_var(name);
	if (v != NULL) {
		free(v->v_value);
		v->v_value = strdup(value);
		return;
	}

	len = strlen(value);

	if ((*value == '"' && value[len - 1] == '"') ||
	    (*value == '\'' && value[len - 1] == '\'')) {
		value[len - 1] = '\0';
		value++;
		len -=2;
	}

	v = (variable_t *)malloc(sizeof(*v));
	if (v == NULL)
		return;
	v->v_name = strdup(name);
	v->v_value = strdup(value);
	v->v_next = vtop;
	vtop = v;
}
