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
#include <string.h>
#include <stdlib.h>
#include <zphoto.h>
#include "config.h"

ZphotoAlist *
zphoto_alist_add (ZphotoAlist *alist, const char *key, const char *value)
{
    ZphotoAlist *new;

    new = zphoto_emalloc(sizeof(ZphotoAlist));
    new->key    = zphoto_strdup(key);
    if (value)
        new->value  = zphoto_strdup(value);
    else
        new->value  = NULL;
    new->next   = NULL;

    if (alist)
	new->next = alist;
    return new;
}

char *
zphoto_alist_get (ZphotoAlist *alist, const char *key)
{
    while (alist) {
	ZphotoAlist *next = alist->next;
	if (strcmp(alist->key, key) == 0)
	    return alist->value;
	alist = next;
    }
    return NULL;
}

void 
zphoto_alist_destroy (ZphotoAlist *alist)
{
    while (alist) {
	ZphotoAlist *next = alist->next;
	free(alist->key);
        if (alist->value)
            free(alist->value);
	free(alist);
	alist = next;
    }
}

