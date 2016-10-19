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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <assert.h>
#include <zphoto.h>
#include "config.h"

static char *packagename = PACKAGE;

static void
xprintf_console (const char *fmt, va_list args)
{
    fflush(stdout);
    if (packagename != NULL)
	fprintf(stderr, "%s: ", packagename);

    vfprintf(stderr, fmt, args);

    if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
	fprintf(stderr, " %s", strerror(errno));
    fprintf(stderr, "\n");
    fflush(stderr);
}

ZphotoXprintfFunc xprintf = xprintf_console;

void
zphoto_set_xprintf (ZphotoXprintfFunc func)
{
    xprintf = func;
}

void
zphoto_eprintf (const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    xprintf(fmt, args);
    va_end(args);
    exit(2);
}


void
zphoto_wprintf (const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    xprintf(fmt, args);
    va_end(args);
}

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
/*
 * They have the declaration of vasprintf in stdio.h
 */
#else
extern int vasprintf (char **ptr, const char *fmt, ...);
#endif

char *
zphoto_asprintf (const char *fmt, ...)
{
    char *str;
    int val;
    va_list args;

    va_start(args, fmt);
    val = vasprintf(&str, fmt, args);
    va_end(args);

    if (val == -1)
	zphoto_eprintf("vasprintf of %s failed:", fmt);

    return str;
}


FILE *
zphoto_efopen (const char *file_name, const char *mode)
{
    FILE *fp = fopen(file_name, mode);
    if (fp == NULL)
	zphoto_eprintf("%s:", file_name);
    return fp;
}

void *
zphoto_emalloc (size_t n)
{
    void *p = malloc(n);
    if (p == NULL)
	zphoto_eprintf("malloc of %u bytes failed:", n);
    return p;
}

int
zphoto_directory_p (const char *dir_name)
{
    struct stat st;
    if (stat(dir_name, &st) == 0) {
        return S_ISDIR(st.st_mode);
    } else {
        return 0;
    }
}


#ifdef __MINGW32__
#  define mkdir(dir_name, mode) mkdir(dir_name)
#endif
void
zphoto_mkdir (const char *dir_name)
{
    if (!zphoto_directory_p(dir_name))
	if (mkdir(dir_name, 0777) == -1)
	    zphoto_eprintf("%s:", dir_name);
}

time_t
zphoto_get_mtime (const char *file_name)
{
    struct stat sb;
    if (stat(file_name, &sb))
	zphoto_eprintf("%s:", file_name);
    return sb.st_mtime;
}

char *
zphoto_strdup (const char *str)
{
    char *p = strdup(str);
    if (p == NULL) {
	zphoto_eprintf("strdup failed:");
    }
    return p;
}

int
zphoto_file_p (const char *file_name)
{
    struct stat sb;
    if (stat(file_name, &sb))
	return 0;
    return S_ISREG(sb.st_mode);
}

static int
executable_file_p (const char *file_name)
{
    struct stat sb;
    if (stat(file_name, &sb))
	return 0;
#ifdef __MINGW32__
    return S_ISREG(sb.st_mode) && (sb.st_mode & S_IEXEC);
#else
    return S_ISREG(sb.st_mode) && 
        (sb.st_mode & S_IXUSR) &&
        (sb.st_mode & S_IXGRP) &&
        (sb.st_mode & S_IXOTH);
#endif
}

DIR *
zphoto_eopendir (const char *dir_name)
{
    DIR *dir = opendir(dir_name);
    if (dir == NULL)
	zphoto_eprintf("%s:", dir_name);
    return dir;
}

char *
zphoto_time_string (time_t time)
{
    static char time_string[BUFSIZ];
    struct tm *tm;
    tm = localtime(&time);
    strftime(time_string, BUFSIZ, "%Y-%m-%d %H:%M:%S", tm);
    return time_string;
}

char *
zphoto_get_suffix (const char *file_name)
{
    char *p = strrchr(file_name, '.');
    if (p) return p+1;
    return NULL;
}

char *
zphoto_suppress_suffix (char *file_name)
{
    char *p = strrchr(file_name, '.');
    if (p) *p = '\0';
    return file_name;
}

char *
zphoto_modify_suffix (const char *file_name, const char *suffix)
{
    char *tmp, *new;
    tmp = zphoto_strdup(file_name);
    zphoto_suppress_suffix(tmp);
    new = zphoto_asprintf("%s.%s", tmp, suffix);
    free(tmp);
    return new;
}

#ifdef __MINGW32__
#include <windows.h>

static int
shift_jis_first_byte_p (int c)
{
    return (c >= 0x81 && c <= 0x9f) || 
           (c >= 0xe0 && c <= 0xfc);
}

static int
shift_jis_second_byte_p (int c)
{
    return (c >= 0x40 && c <= 0x7e) || 
           (c >= 0x80 && c <= 0xfc);
}

static char *
find_last_separator_jpn (const char *file_name)
{
    unsigned char *p, *separator = NULL;
    for (p = (unsigned char *)file_name; *p != '\0'; p++) {
        if (shift_jis_first_byte_p(*p)) {
            p++;
            if (! shift_jis_second_byte_p(*p))
                zphoto_eprintf("broken Shift_JIS: %s", file_name);
        } else if (*p == '/' || *p == '\\') {
            separator = p;
        }
    }
    return (char *)separator;
}

static char *
find_last_separator (const char *file_name)
{
    if (zphoto_platform_w32_jpn_p()) {
        return find_last_separator_jpn(file_name);
    } else {
        unsigned char *p, *separator = NULL;
        for (p = (unsigned char *)file_name; *p != '\0'; p++) {
            if (*p == '/' || *p == '\\')
                separator = p;
        }
        return (char *)separator;
    }
}


#else

static char *
find_last_separator (const char *file_name)
{
    return strrchr(file_name, '/');
}

#endif

static int
root_p (const char *file_name)
{
    if (zphoto_platform_w32_p()) {
        if (strcmp(file_name, "\\") == 0) {
            return 1;
        } else if (strcmp(file_name, "/") == 0) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return strcmp(file_name, "/") == 0;
    }
}

char *
zphoto_basename (const char *file_name)
{
    char *p;

    if (root_p(file_name)) {
        return (char *)file_name;
    } else if ((p = find_last_separator(file_name))) {
        return p + 1;
    } else {
        return (char *)file_name;
    }
}

char *
zphoto_dirname (const char *file_name)
{
    char *p;

    if (root_p(file_name)) {
        return zphoto_strdup(file_name);
    } else if ((p = find_last_separator(file_name))) {
        char *dirname = zphoto_strdup(file_name);
        dirname[p - file_name] = '\0';
        return dirname;
    } else {
        return zphoto_strdup(file_name);
    }
}

static char *
downcase (const char *str)
{
    char *new = zphoto_strdup(str);
    char *p = new;

    while (*p) {
	*p = tolower(*p);
	p++;
    }
    return new;
}

/*
 * 'x' to avoid confliction.
 */
static char *
xstrcasestr (const char *str1, const char *str2)
{
    char *p1  = downcase(str1);
    char *p2  = downcase(str2); 
    char *val = strstr(p1, p2);

    free(p1);
    free(p2);
    return val;
}

int
zphoto_strsuffixcasecmp (const char *str1, const char *str2)
{
    int len1, len2;

    len1 = strlen(str1);
    len2 = strlen(str2);

    if (len1 > len2) {
        return strcasecmp(str1 + len1 - len2, str2);
    } else {                                         
        /* return strcasecmp(str2 + len2 - len1, str1); */
        return -1;
    }
}

char **
zphoto_get_image_suffixes (void)
{
    static char *empty[] = { NULL };
    static char *suffixes[] = { "jpg", "png", "gif", "bmp", NULL };

    if (zphoto_support_image_p()) {
        return suffixes;
    } else {
        return empty;
    }
}

char **
zphoto_get_movie_suffixes (void)
{
    static char *empty[] = { NULL };
    static char *suffixes[] = { "avi", "mpg", NULL };

    if (zphoto_support_movie_p()) {
        return suffixes;
    } else {
        return empty;
    }
}

static int
match_suffix (const char *file_name, char **suffixes)
{
    char **p = suffixes;
    while (*p != NULL) {
        char *tmp = zphoto_asprintf(".%s", *p);
        if (zphoto_strsuffixcasecmp(file_name, tmp) == 0 ) {
            free(tmp);
            return 1;
        }
        free(tmp);
        p++;
    }
    return 0;
}

/* 
 * Check by its file name only (not its content). 
 */
int
zphoto_supported_file_p (const char *file_name)
{
    return (zphoto_support_image_p() && zphoto_image_file_p(file_name)) ||
           (zphoto_support_movie_p() && zphoto_movie_file_p(file_name));
}

int
zphoto_image_file_p (const char *file_name)
{
    return match_suffix(file_name, zphoto_get_image_suffixes());
}

int
zphoto_movie_file_p (const char *file_name)
{
    return match_suffix(file_name, zphoto_get_movie_suffixes());
}

int
zphoto_web_file_p (const char *file_name)
{

    if ((zphoto_strsuffixcasecmp(file_name, ".html")  == 0) ||
        (zphoto_strsuffixcasecmp(file_name, ".css")   == 0) ||
        (zphoto_strsuffixcasecmp(file_name, ".js")   == 0) ||
        (xstrcasestr(file_name, ".html.")) || /* multiview: index.html.ja */
        (xstrcasestr(file_name, ".js.")))    /* multiview: foo.js.ja */
    {
	return 1;
    } else {
	return 0;
    }
}

int
zphoto_dot_file_p (const char *file_name)
{
    if (zphoto_basename(file_name)[0] == '.')
        return 1;
    else
        return 0;
}

int
zphoto_path_exist_p (const char *file_name)
{
    struct stat st;
    if (stat(file_name, &st) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int
zphoto_support_movie_p (void)
{
#if HAVE_AVIFILE && HAVE_IMLIB2
    return 1;
#else
    return 0;
#endif
}

int
zphoto_support_image_p (void)
{
#if HAVE_IMLIB2 || HAVE_MAGICK
    return 1;
#else
    return 0;
#endif
}

int
zphoto_support_zip_p (void)
{
#ifdef HAVE_ZIP
    return 1;
#else
    return 0;
#endif
}

int
zphoto_platform_w32_p (void)
{
#ifdef __MINGW32__
    return 1;
#else
    return 0;
#endif
}

int
zphoto_platform_w32_jpn_p (void) 
{
#ifdef __MINGW32__
    char locale_name[32];
    GetLocaleInfo(LOCALE_USER_DEFAULT, 
                  LOCALE_SABBREVLANGNAME | LOCALE_USE_CP_ACP, 
                  locale_name, sizeof(locale_name));
    return strcmp(locale_name, "JPN") == 0;
#else
    return 0;
#endif
}

char *
zphoto_get_program_file_name (void)
{
#ifdef __MINGW32__
    int length, maxlen = 1024;
    char file_name[maxlen];
    length = GetModuleFileName(NULL, file_name, maxlen);
    file_name[length] = '\0';
    return zphoto_strdup(file_name);
#else
    assert(0); /* unsupported */
    return NULL;
#endif
}


/*
 * ImageMagick depended codes.
 */
#ifdef HAVE_MAGICK
#include <magick/api.h>

void
zphoto_init_magick (void)
{
    InitializeMagick(PACKAGE);
}

void
zphoto_finalize_magick (void)
{
    DestroyMagick();
}

static int
magick_has_readdir_bug_p (void)
{
    return MagickLibVersion <= 0x557;
}

#else
void
zphoto_init_magick (void)
{
}

void
zphoto_finalize_magick (void)
{
}

static int
magick_has_readdir_bug_p (void)
{
    return 0;
}
#endif

char *
zphoto_d_name_workaround (struct dirent *d)
{
    /*
     * FIXME: very very dirty workaround for ImageMagick's own
     * opendir/readdir in magick/nt_base.c.
     * I've reported the bug in 2004-04-02 but for the moment...
     */
    if (zphoto_platform_w32_p() && magick_has_readdir_bug_p()) {
        return d->d_name - 8;
    } else {
        return d->d_name;
    }
}

int
zphoto_blank_line_p (const char *line)
{
    return line[strspn(line, "\t ")] == '\0';
}

int
zphoto_complete_line_p (const char *line)
{
    return line[strlen(line) - 1] == '\n';
}

void
zphoto_chomp (char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

int 
zphoto_valid_color_string_p (const char *string)
{
    const char *p;
    int len = strlen(string);

    if (!(string[0] == '#' && (len == 9 || len == 7)))
        return 0;

    p = string + 1;
    while (*p != '\0') {
        if (!isxdigit(*p))
            return 0;
        p++;
    }

    return 1;
}

void
zphoto_decode_color_string (const char *string, int *r, int *g, int *b, int *a)
{
    unsigned long int value;
    int len;

    if (!zphoto_valid_color_string_p(string))
        zphoto_eprintf("%s: malformed color value", string);

    value = strtoul(string + 1, NULL, 16);
    len = strlen(string);
    if (len ==  9) { /* has alpha */
        *r = (value >> 24) & 255;
        *g = (value >> 16) & 255;
        *b = (value >>  8) & 255;
        *a = value & 255;
    } else {
        *r = (value >> 16) & 255;
        *g = (value >>  8) & 255;
        *b = value & 255;
        *a = -1;
    }
}

char *
zphoto_encode_color_string (int r, int g, int b, int a)
{
    if (a > 0) {
        return zphoto_asprintf("#%02x%02x%02x%02x", r, g, b, a);
    } else {
        return zphoto_asprintf("#%02x%02x%02x", r, g, b);
    }
}

static char *
find_browser (void)
{
    char *browsers[] = { 
        "/usr/bin/x-www-browser", /* debian */
        "/usr/bin/htmlview",      /* red hat */
        NULL
    };
    char **p = browsers;

    while (*p != NULL) {
        if (executable_file_p(*p)) {
            return *p;
        }
        p++;
    }
    return NULL;
}

int
zphoto_support_browser_p (void)
{
    if (zphoto_platform_w32_p()) {
        return 1;
    } else {
        if (find_browser() != NULL) {
            return 1;
        } else {
            return 0;
        }
    }
}

void
zphoto_launch_browser (const char *url)
{
#ifdef __MINGW32__
    ShellExecute(NULL, "open", url, NULL, "", SW_SHOWNORMAL);
#else
    if (zphoto_support_browser_p()) {
        char *browser = find_browser();
        char *command = zphoto_asprintf("%s %s &", browser, url);
        system(command);
        free(command);
    }
#endif
}

int
zphoto_path_separator (void)
{
    if (zphoto_platform_w32_p()) {
        return '\\';
    } else {
        return '/';
    }
}

static int
has_drive_letter (const char *path)
{
    return isalpha(*path) && path[1] == ':';
}

static int
relative_path_p (const char *path)
{
    if (zphoto_platform_w32_p()) {
        return !(*path == '/' || *path == '\\' || has_drive_letter(path));
    } else {
        return *path != '/';
    }
}

/*
 * FIXME: Very ad hoc implementation. 
 */
char *
zphoto_expand_path (const char *path, const char *dir_name)
{
    char current_dir[BUFSIZ];
    if (relative_path_p(path)) {
        if (dir_name == NULL) {
            getcwd(current_dir, BUFSIZ);
            dir_name = current_dir;
        }
        return zphoto_asprintf("%s%c%s", 
                               dir_name,
                               zphoto_path_separator(),
                               path);
    } else {
        return zphoto_strdup(path);
    }
}

int
zphoto_directory_empty_p (const char *dir_name)
{
    int count = 0;
    DIR *dir = zphoto_eopendir(dir_name);
    struct dirent *d;

    while ((d = readdir(dir))) {
        count++;
    }
    closedir(dir);
    return count == 2; /* "." and ".." */
}

char *
zphoto_escape_url (const char *url)
{
    const char *p;
    char *new_url = zphoto_emalloc(strlen(url) * 3 + 1), *pp;

    for (p = url, pp = new_url; *p != '\0'; p++, pp++) {
        /*
         * We intentionally don't apply the following conversion.
         * if (*p == ' ') { *pp = '+'; }
         */
        if ((isascii(*p) && isalnum(*p)) || 
                   (strchr("_.-", *p) != NULL)) {
            *pp = *p;
        } else {
            snprintf(pp, 4, "%%%02X", (unsigned char)*p);
            pp += 2;
        }
    }
    *pp = '\0';
    return new_url;
}


