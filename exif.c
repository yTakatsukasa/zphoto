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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include "zphoto.h"
#include "config.h"

enum {
    HEADER_OFFSET1 = 12,
    HEADER_OFFSET2 = 8
};

#define SWAP_ENDIAN_LONG(val) ((uint32_t) ( \
    (((uint32_t) (val) & (uint32_t) 0x000000ffU) << 24) | \
    (((uint32_t) (val) & (uint32_t) 0x0000ff00U) <<  8) | \
    (((uint32_t) (val) & (uint32_t) 0x00ff0000U) >>  8) | \
    (((uint32_t) (val) & (uint32_t) 0xff000000U) >> 24)))

#define SWAP_ENDIAN_SHORT(val) ((uint16_t) ( \
    (((uint16_t) (val) & (uint16_t) 0x00ff) << 8) | \
    (((uint16_t) (val) & (uint16_t) 0xff00) >> 8)))

static int 
is_little_endian ()
{
    static long retval = -1;

    if (retval == -1) {
	uint32_t n = 1;
	char *p = (char *)&n;
	char x[] = {1, 0, 0, 0};

	assert(sizeof(uint32_t) == 4);
	if (memcmp(p, x, 4) == 0) {
	    retval = 1;
	} else {
	    retval = 0;
	}
    }
    return retval;
}

static unsigned long
ulong_from_le (unsigned long x)
{
    if (is_little_endian()) {
	return x;
    } else {
	return SWAP_ENDIAN_LONG(x);
    }
}

static unsigned short
ushort_from_be (unsigned short x)
{
    if (is_little_endian()) {
	return SWAP_ENDIAN_SHORT(x);
    } else {
	return x;
    }
}

static unsigned long
ulong_from_be (unsigned long x)
{
    if (is_little_endian()) {
	return SWAP_ENDIAN_LONG(x);
    } else {
	return x;
    }
}

static unsigned short
ushort_from_le (unsigned short x)
{
    if (is_little_endian()) {
	return x;
    } else {
	return SWAP_ENDIAN_SHORT(x);
    }
}

static size_t 
efread (void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t n;
    n = fread(ptr, size, nmemb, stream);
    if (n != nmemb)
        zphoto_eprintf("fread failed:");
    return n;
}


static int
efseek (FILE *stream, long offset, int whence)
{
    int x = fseek(stream, offset, whence);
    if (x != 0)
        zphoto_eprintf("fseek failed:");
    return x;
}

int 
zphoto_exif_file_p (const char *file_name)
{
    unsigned char exif_header[] = { 0xff, 0xd8, 0xff, 0xe1 };
    char magic[4];
    FILE *fp = zphoto_efopen(file_name, "rb");
    int n;
    
    n = fread(magic, sizeof(char), 4, fp);
    fclose(fp);
    if (n == 4 && memcmp(magic, exif_header, 4) == 0)
        return 1;
    else
        return 0;
}

typedef void	(*TraverseFunc)	 (FILE *fp,
                                  unsigned short tag, 
                                  unsigned short type, 
                                  unsigned long size, 
                                  unsigned long value, 
                                  void *data);

static void
get_special_offset (FILE *fp, 
                    unsigned short tag, 
                    unsigned short type, 
                    unsigned long size, 
                    unsigned long value, 
                    void *data)
{
    if (tag == 0x8769) {
        *(int *)data = value;
    }
}

static void
get_time (FILE *fp, 
          unsigned short tag,
          unsigned short type,
          unsigned long size, 
          unsigned long value, 
          void *data)
{
    if (tag == 0x9003) {
        long curpos;
        char buf[BUFSIZ];
        struct tm t;

        curpos = ftell(fp);
        efseek(fp, HEADER_OFFSET1 + value, SEEK_SET);
        assert(size < BUFSIZ);
        efread(buf, sizeof(char),  size, fp); /* 2003:01:26 16:37:04 */

        buf[ 4] = buf[ 7] = buf[10] = buf[13] =  buf[16] = buf[19] = '\0';

        memset(&t, 0, sizeof(struct tm)); /* Clear it for safety */
        t.tm_year = atoi(buf) - 1900;
        t.tm_mon  = atoi(buf + 5) - 1;
        t.tm_mday = atoi(buf + 8);
        t.tm_hour = atoi(buf + 11);
        t.tm_min  = atoi(buf + 14);
        t.tm_sec  = atoi(buf + 17);

        *(time_t *)data = mktime(&t);
        efseek(fp, curpos, SEEK_SET);
    }
}

static unsigned short
read_ushort (FILE *fp, int le_exif_p)
{
    unsigned short x;

    efread(&x, sizeof(uint16_t), 1, fp);
    if (le_exif_p)
        return ushort_from_le(x);
    else
        return ushort_from_be(x);
}

static unsigned long
read_ulong (FILE *fp, int le_exif_p)
{
    uint32_t x;

    efread(&x, sizeof(uint32_t), 1, fp);
    if (le_exif_p)
        return ulong_from_le(x);
    else
        return ulong_from_be(x);
}

static int
read_directory (FILE *fp, TraverseFunc func, void *data, int le_exif_p)
{
    int i;
    unsigned short n;

    n = read_ushort(fp, le_exif_p);
    for (i = 0; i < n; i++) {
        unsigned short tag, type;
        unsigned long size, value;

        tag   = read_ushort(fp, le_exif_p);
        type  = read_ushort(fp, le_exif_p);
        size  = read_ulong(fp, le_exif_p);
        value = read_ulong(fp, le_exif_p);

        func(fp, tag, type, size, value, data);
    }
    return 0;
}

static int
exif_little_endian_exif_p (const char *file_name, FILE *fp)
{
    char buf[2];
    char little_endian_magic[] = { 0x49, 0x49 };
    char big_endian_magic[]    = { 0x4d, 0x4d };

    efseek(fp, HEADER_OFFSET1, SEEK_SET);
    efread(buf, sizeof(char), 2, fp);
    if (memcmp(buf, little_endian_magic, 2) == 0) {
        return 1;
    } else if (memcmp(buf, big_endian_magic, 2) == 0) {
        return 0;
    } else {
        zphoto_eprintf("%s: unknown EXIF format", file_name);
        return 0; /* not reached */
    }
}

time_t
zphoto_exif_get_time (const char *file_name)
{
    time_t t = -1;
    
    if (zphoto_exif_file_p(file_name)) {
        int special_offset = -1;
        FILE *fp = zphoto_efopen(file_name, "rb");
        int le_exif_p = exif_little_endian_exif_p(file_name, fp);

        efseek(fp, HEADER_OFFSET1 + HEADER_OFFSET2, SEEK_SET);
        read_directory(fp, get_special_offset, 
                       &special_offset, le_exif_p);
        if (special_offset != -1) {
            efseek(fp, HEADER_OFFSET1 + special_offset, SEEK_SET);
            read_directory(fp, get_time, &t, le_exif_p);
        }
        fclose(fp);
    }
    if (t == -1) {
        t = zphoto_get_mtime(file_name);
    }
    return t;
}
