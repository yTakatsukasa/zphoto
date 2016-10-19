/* 
 * zphoto - a zooming photo album generator.
 *
 * Copyright (C) 2002-2004  Satoru Takabayashi <satoru@namazu.org>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zphoto.h>
#include "config.h"

struct _ZphotoTemplate {
    char *file_name;
    char *content;
    ZphotoAlist *alist;
};

static size_t
get_file_size (const char *file_name)
{
    struct stat s;
    if (stat(file_name, &s) == -1)
	zphoto_eprintf("%s:", file_name);
    return s.st_size;
}

static char *
read_file (const char *file_name)
{
    FILE *fp;
    char *content;
    size_t file_size;

    file_size = get_file_size(file_name);

    fp  = zphoto_efopen(file_name, "rb");
    content = zphoto_emalloc(file_size + 1);

    fread(content, sizeof(char), file_size, fp);
    if (ferror(fp))
	zphoto_eprintf("%s:", file_name);

    content[file_size] = '\0';
    fclose(fp);
    return content;
}

static char *
substitute_region (ZphotoTemplate *template, 
		   const char *start, 
		   const char *end, 
		   FILE *fp)
{
    int len;
    char *key, *value;

    len = end - start;
    key = zphoto_emalloc(len + 1);
    strncpy(key, start, len);
    key[len] = '\0';

    value = zphoto_alist_get(template->alist, key);
    free(key);
    return value;
}

static void
substituting_print (ZphotoTemplate *template, FILE *fp)
{
    const char *p = template->content;

    while (*p != '\0') {
	if (strncmp(p, "#{", 2) == 0) {
	    char *q, *value;
	    p += 2;
	    q = strchr(p, '}');
	    if (q == NULL)
		zphoto_eprintf("%s: unclosed brace at %d",
			       template->file_name, p - template->content);
	    
	    value = substitute_region(template, p, q, fp);
	    if (value)
		fprintf(fp, "%s", value);
	    p = q + 1;
	} else {
	    fputc(*p, fp);
	    p++;
	}
    }
}

void
zphoto_template_write (ZphotoTemplate *template, const char *output_file_name)
{
    FILE *fp = zphoto_efopen(output_file_name, "wb");
    substituting_print(template, fp);
    fclose(fp);
}

void
zphoto_template_add_subst (ZphotoTemplate *template, 
			     const char *key,
			     const char *value)
{
    template->alist = zphoto_alist_add(template->alist, key, value);
}

ZphotoTemplate *
zphoto_template_new (const char *file_name)
{
    ZphotoTemplate *template;

    template = zphoto_emalloc(sizeof(ZphotoTemplate));
    template->file_name = zphoto_strdup(file_name);
    template->alist = NULL;
    template->content = read_file(template->file_name);
    return template;
}

void
zphoto_template_destroy (ZphotoTemplate *template)
{
    zphoto_alist_destroy(template->alist);
    free(template->file_name);
    free(template->content);
    free(template);
}

