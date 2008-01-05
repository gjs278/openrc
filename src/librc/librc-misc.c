/*
   rc-misc.c
   rc misc functions
   */

/* 
 * Copyright 2007 Roy Marples
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "librc.h"

bool rc_yesno (const char *value)
{
	if (! value) {
		errno = ENOENT;
		return (false);
	}

	if (strcasecmp (value, "yes") == 0 ||
		strcasecmp (value, "y") == 0 ||
		strcasecmp (value, "true") == 0 ||
		strcasecmp (value, "1") == 0)
		return (true);

	if (strcasecmp (value, "no") != 0 &&
		strcasecmp (value, "n") != 0 &&
		strcasecmp (value, "false") != 0 &&
		strcasecmp (value, "0") != 0)
		errno = EINVAL;

	return (false);
}
librc_hidden_def(rc_yesno)

char *rc_strcatpaths (const char *path1, const char *paths, ...)
{
	va_list ap;
	int length;
	int i;
	char *p;
	char *path;
	char *pathp;

	if (! path1 || ! paths)
		return (NULL);

	length = strlen (path1) + strlen (paths) + 1;
	if (*paths != '/')
		length ++;

	va_start (ap, paths);
	while ((p = va_arg (ap, char *)) != NULL) {
		if (*p != '/')
			length ++;
		length += strlen (p);
	}
	va_end (ap);

	pathp = path = xmalloc (length * sizeof (char));
	memset (path, 0, length);
	i = strlen (path1);
	memcpy (path, path1, i);
	pathp += i;
	if (*paths != '/')
		*pathp ++ = '/';
	i = strlen (paths);
	memcpy (pathp, paths, i);
	pathp += i;

	va_start (ap, paths);
	while ((p = va_arg (ap, char *)) != NULL) {
		if (*p != '/')
			*pathp ++= '/';
		i = strlen (p);
		memcpy (pathp, p, i);
		pathp += i;
	}
	va_end (ap);

	*pathp++ = 0;

	return (path);
}
librc_hidden_def(rc_strcatpaths)


char **rc_config_load (const char *file)
{
	char **list = NULL;
	FILE *fp;
	char *buffer;
	char *p;
	char *token;
	char *line;
	char *linep;
	char *linetok;
	int i = 0;
	bool replaced;
	char *entry;
	char *newline;

	if (! (fp = fopen (file, "r"))) 
		return (NULL);

	buffer = xmalloc (sizeof (char) * RC_LINEBUFFER);
	while (fgets (buffer, RC_LINEBUFFER, fp)) {
		p = buffer;

		/* Strip leading spaces/tabs */
		while ((*p == ' ') || (*p == '\t'))
			p++;

		if (! p || strlen (p) < 3 || p[0] == '#')
			continue;

		/* Get entry */
		token = strsep (&p, "=");

		if (! token)
			continue;

		entry = xstrdup (token);

		/* Preserve shell coloring */
		if (*p == '$')
			token = p;
		else
			do {
				/* Bash variables are usually quoted */
				token = strsep (&p, "\"\'");
			} while ((token) && (strlen (token) == 0));

		/* Drop a newline if that's all we have */
		i = strlen (token) - 1;
		if (token[i] == 10)
			token[i] = 0;

		i = strlen (entry) + strlen (token) + 2;
		newline = xmalloc (i);
		snprintf (newline, i, "%s=%s", entry, token);

		replaced = false;
		/* In shells the last item takes precedence, so we need to remove
		   any prior values we may already have */
		STRLIST_FOREACH (list, line, i) {
			char *tmp = xstrdup (line);
			linep = tmp; 
			linetok = strsep (&linep, "=");
			if (strcmp (linetok, entry) == 0) {
				/* We have a match now - to save time we directly replace it */
				free (list[i - 1]);
				list[i - 1] = newline;
				replaced = true;
				free (tmp);
				break;
			}
			free (tmp);
		}

		if (! replaced) {
			rc_strlist_addsort (&list, newline);
			free (newline);
		}
		free (entry);
	}
	free (buffer);
	fclose (fp);

	return (list);
}
librc_hidden_def(rc_config_load)

char *rc_config_value (char **list, const char *entry)
{
	char *line;
	int i;
	char *p;

	STRLIST_FOREACH (list, line, i) {
		p = strchr (line, '=');
		if (p && strncmp (entry, line, p - line) == 0)
			return (p += 1);
	}

	return (NULL);
}
librc_hidden_def(rc_config_value)

char **rc_config_list (const char *file)
{
	FILE *fp;
	char *buffer;
	char *p;
	char *token;
	char **list = NULL;

	if (! (fp = fopen (file, "r")))
		return (NULL);

	buffer = xmalloc (sizeof (char) * RC_LINEBUFFER);
	while (fgets (buffer, RC_LINEBUFFER, fp)) {
		p = buffer;

		/* Strip leading spaces/tabs */
		while ((*p == ' ') || (*p == '\t'))
			p++;

		/* Get entry - we do not want comments */
		token = strsep (&p, "#");
		if (token && (strlen (token) > 1)) {
			/* Stip the newline if present */
			if (token[strlen (token) - 1] == '\n')
				token[strlen (token) - 1] = 0;

			rc_strlist_add (&list, token);
		}
	}
	free (buffer);
	fclose (fp);

	return (list);
}
librc_hidden_def(rc_config_list)