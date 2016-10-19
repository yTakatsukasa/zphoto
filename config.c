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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <popt.h>
#include <unistd.h>
#include <zphoto.h>
#include "config.h"

static char *
replace_path_separator (const char *path)
{
    char *new = zphoto_strdup(path);
    if (zphoto_platform_w32_p()) {
        char *p = new;
        while (*p != '\0') {
            if (*p == '/')
                *p = '\\';
            p++;
        }
    }
    return new;
}

/*
 * Returns a newly created string.
 */
static char *
choose_path (int npaths, ...)
{
    int i;
    char *path = NULL;

    va_list args;
    va_start(args, npaths);
    for (i = 0; i < npaths - 1; i++) {
        path = va_arg(args, char *);
        if (path && zphoto_path_exist_p(path)) {
            return replace_path_separator(path);
        }
    }
    path = va_arg(args, char *); /* use the last one */
    va_end(args);
    return replace_path_separator(path);
}

static void
meta_config_destroy (MetaConfig *meta)
{
    while (meta) {
	MetaConfig *next = meta->next;
	free(meta->key);
        free(meta->type);
        free(meta->long_option);
        free(meta->help);
        if (meta->help_arg)
            free(meta->help_arg);
	free(meta);
	meta = next;
    }
}

static MetaConfig *
meta_config_get (MetaConfig *rclist, const char *key)
{
    while (rclist) {
	MetaConfig *next = rclist->next;
	if (strcmp(rclist->key, key) == 0)
	    return rclist;
	rclist = next;
    }
    return NULL;
}

static char *
underscore_to_hyphen (const char *str)
{
    char *new = zphoto_strdup(str);
    char *p = new;
    while (*p != '\0') {
        if (*p == '_')
            *p = '-';
        p++;
    }
    return new;
}

static MetaConfig *
meta_config_reverse (MetaConfig *meta, MetaConfig *prev)
{
    MetaConfig *next = meta->next;
    meta->next = prev;
    if (next) {
        return meta_config_reverse(next, meta);
    } else {
        return meta;
    }
}

static int
meta_config_count (MetaConfig *meta)
{
    int n = 0;
    while (meta) {
	meta = meta->next;
        n++;
    }
    return n;
}

static MetaConfig *
meta_config_add (MetaConfig *meta, 
                 const char *key, 
                 void *storage, 
                 const char *type,
                 const char *long_option,
                 int   short_option,
                 const char *help,
                 const char *help_arg)
{
    MetaConfig *new;

    new = zphoto_emalloc(sizeof(MetaConfig));
    new->key          = zphoto_strdup(key);
    new->storage      = storage;
    new->type         = zphoto_strdup(type);
    new->long_option  = zphoto_strdup(long_option);
    new->short_option = short_option;
    new->help         = zphoto_strdup(help);
    new->help_arg     = help_arg ? zphoto_strdup(help_arg) : NULL;
    new->next         = NULL;

    if (meta)
	new->next = meta;
    return new;
}

#define set_config(config, key, value, type, short_option, help, help_arg) { \
    char *long_option = underscore_to_hyphen(#key); \
    config->key  = value; \
    config->meta = meta_config_add(config->meta, #key, &config->key, #type, \
                                   long_option, short_option, help, help_arg); \
    free(long_option); \
}

static char *
rewrite_lang (const char *path)
{
    /*
     * FIXME: Kludge for Japanese Windows users.
     */
    if (zphoto_platform_w32_jpn_p() && 
        zphoto_strsuffixcasecmp(path, "en") == 0) 
    {
        char *new_path, *dirname = zphoto_dirname(path);
        new_path = zphoto_asprintf("%s/ja", dirname);
        free(dirname);
        return new_path; /* FIXME: The value will never be freed. */
        
    } else {
        return (char *)path;
    }
}

ZphotoConfig *
zphoto_config_new (void)
{
    char *template_dir, *flash_font;
    ZphotoConfig *config = zphoto_emalloc(sizeof(ZphotoConfig));
    config->meta = NULL;

    template_dir = choose_path(3, 
                               getenv("ZPHOTO_TEMPLATE_DIR"), 
                               rewrite_lang(ZPHOTO_TEMPLATE_DIR),
                               rewrite_lang(ZPHOTO_TEMPLATE_DIR_RELATIVE));
    flash_font = choose_path(3, 
                             getenv("ZPHOTO_FONT"), 
                             ZPHOTO_FONT,
                             ZPHOTO_FONT_RELATIVE);
    config->zphoto_url = ZPHOTO_URL;

    set_config(config, photo_width, 600, int,
               'w', "resize photos with WIDTH for preview", "WIDTH");
    set_config(config, thumbnail_width, 320, int,
               '\0', "make thumbnails of WIDTH", "WIDTH");
    set_config(config, html_thumbnail_width, 120, int,
               '\0', "use WIDTH in noflash.html", "WIDTH");
    set_config(config, gamma, 1.0, float,
               'g', "apply gamma correction (0.0-10.0)", "GAMMA");
    set_config(config, output_dir, "zphoto-album", string,
               'o', "output files to DIR", "DIR");
    set_config(config, template_dir, template_dir, string,
               't', "make HTML files using templates in DIR", "DIR");
    set_config(config, flash_font  , flash_font, string,
               'f', "use FONT for Flash", "FONT");
    set_config(config, flash_width , 750, int,
               '\0', "set flash's width to WIDTH", "WIDTH");
    set_config(config, flash_height, 500, int,
               '\0', "set flash's height to HEIGHT", "HEIGHT");
    set_config(config, flash_filename, "zphoto.swf", string,
               '\0', "set the output flash file name to FILE", "FILE");
    set_config(config, movie_nsamples, 4, int,
               '\0', "use NUM of  frames for a movie thumbnail", "NUM");
    set_config(config, title, "", string,
               '\0', "set HTML's <title> to TITLE", "TITLE");
    set_config(config, caption_file, "", string,
               '\0', "use caption table file for photo captions", "PATH");
    set_config(config, html_suffix, "", string,
               '\0', "set photo HTML's suffix to SUFFIX", "SUFFIX");
    set_config(config, preview_prefix, "pv-", string,
               '\0', "set preview file's prefix to PREFIX", "PREFIX");
    set_config(config, thumbnail_prefix, "tn-", string,
               '\0', "set thumbnail's prefix to PREFIX", "PREFIX");
    set_config(config, zip_filename, "zphoto.zip", string,
               '\0', "set the output zip file name to FILE", "FILE");
    set_config(config, zip_command, "zip -qjg", string,
               '\0', "set zip command to COMMAND", "COMMAND");

    /*
     * Boolean flags
     */
    set_config(config, include_original, 0, bool,
               '\0', "include original photos (works with --width)", NULL);
    set_config(config, sequential, 0, bool,
               '\0', "use sequential file name for image and html files",NULL);
    set_config(config, disable_captions, 0, bool,
               '\0', "do not create a caption for each photo", NULL);
    set_config(config, caption_by_filename, 0, bool,
               '\0', "use file name for photo captions", NULL);
    set_config(config, sort_by_filename, 0, bool,
               '\0', "sort images by file name (instead of date)", NULL);
    set_config(config, no_sort, 0, bool,
               '\0', "do not sort images", NULL);
    set_config(config, no_zip, 0, bool,
               '\0', "do not create a zip file", NULL);
    set_config(config, no_exif, 0, bool,
               '\0', "do not use EXIF information", NULL);
    set_config(config, no_fade, 0, bool,
               '\0', "do not use fade effect for a movie thumbnail",NULL);
    set_config(config, quiet, 0, bool,
               'q', "suppress all normal output", NULL);
    set_config(config, art, 0, bool,
               '\0', "art mode (not for practical use)", NULL);

    set_config(config, background_color, ZPHOTO_BACKGROUND_COLOR, string,
               '\0', "set flash background color to COLOR", "COLOR");
    set_config(config, border_active_color,
               ZPHOTO_BORDER_ACTIVE_COLOR, string,
               '\0', "set border active color to COLOR", "COLOR");
    set_config(config, border_inactive_color,
               ZPHOTO_BORDER_INACTIVE_COLOR, string,
               '\0', "set border inactive color to COLOR", "COLOR");
    set_config(config, shadow_color, ZPHOTO_SHADOW_COLOR, string,
               '\0', "set shadow color to COLOR", "COLOR");
    set_config(config, caption_border_color,
               ZPHOTO_CAPTION_BORDER_COLOR, string,
               '\0', "set caption border color to COLOR", "COLOR");
    set_config(config, caption_frame_color,
               ZPHOTO_CAPTION_FRAME_COLOR, string,
               '\0', "set caption frame color to COLOR", "COLOR");
    set_config(config, caption_text_color, ZPHOTO_CAPTION_TEXT_COLOR, string,
               '\0', "set caption text color to COLOR", "COLOR");
    set_config(config, progress_bar_color, ZPHOTO_PROGRESS_BAR_COLOR, string,
               '\0', "set progress bar color to COLOR", "COLOR");
    set_config(config, progress_bar_text_color,
               ZPHOTO_PROGRESS_BAR_TEXT_COLOR, string,
               '\0', "set progress bar text color to COLOR", "COLOR");
    set_config(config, progress_bar_housing_color, 
               ZPHOTO_PROGRESS_BAR_HOUSING_COLOR, string,
               '\0', "set progress bar housing color to COLOR", "COLOR");

    set_config(config, css_background_color, ZPHOTO_CSS_BACKGROUND_COLOR, string,
               '\0', "set css background color to COLOR", "COLOR");
    set_config(config, css_text_color, ZPHOTO_CSS_TEXT_COLOR, string,
               '\0', "set css text color to COLOR", "COLOR");
    set_config(config, css_text_color, ZPHOTO_CSS_TEXT_COLOR, string,
               '\0', "set css text color to COLOR", "COLOR");
    set_config(config, css_footer_color, ZPHOTO_CSS_FOOTER_COLOR, string,
               '\0', "set css footer color to COLOR", "COLOR");
    set_config(config, css_horizontal_line_color, 
               ZPHOTO_CSS_HORIZONTAL_LINE_COLOR, string,
               '\0', "set css horizontal line color to COLOR", "COLOR");
    set_config(config, css_photo_border_color,
               ZPHOTO_CSS_PHOTO_BORDER_COLOR, string,
               '\0', "set css photo border color to COLOR", "COLOR");
    set_config(config, css_thumbnail_border_color, 
               ZPHOTO_CSS_THUMBNAIL_BORDER_COLOR, string,
               '\0', "set css thumbnail border color to COLOR", "COLOR");
    set_config(config, css_navi_link_color, ZPHOTO_CSS_NAVI_LINK_COLOR, string,
               '\0', "set css navi link color to COLOR", "COLOR");
    set_config(config, css_navi_visited_color, 
               ZPHOTO_CSS_NAVI_VISITED_COLOR, string,
               '\0', "set css navi visited color to COLOR", "COLOR");
    set_config(config, css_navi_border_color, 
               ZPHOTO_CSS_NAVI_BORDER_COLOR, string,
               '\0', "set css navi border color to COLOR", "COLOR");
    set_config(config, css_navi_hover_color, ZPHOTO_CSS_NAVI_HOVER_COLOR, string,
               '\0', "set css navi hover color to COLOR", "COLOR");


    config->meta = meta_config_reverse(config->meta, NULL);
    return config;
}

static void
show_version (void)
{
    printf("%s %s\n", PACKAGE, VERSION);
}

static void
set_option (struct poptOption *options, int i,
            const char *long_option, int short_option, int flag,
            void *storage, int val, const char *help, const char *help_arg)
{
        options[i].longName   = long_option;
        options[i].shortName  = short_option;
        options[i].argInfo    = flag;
        options[i].arg        = storage;
        options[i].val        = val;
        options[i].descrip    = help;
        options[i].argDescrip = help_arg;
}

static struct poptOption *
config_options (ZphotoConfig *config)
{
    int i, n;
    struct poptOption *options;
    MetaConfig *meta;

    n = meta_config_count(config->meta);
    /*
     * FIXME: Be careful to update the value.
     * 5 means --version, --dump-config, --config, --help, NULL.
     */
    options = zphoto_emalloc(sizeof(struct poptOption) * (n + 5)); 

    i = 0;
    for (meta = config->meta; meta; meta = meta->next) {
        int flag = 0;
        if (strcmp(meta->type, "string") == 0) {
            flag = POPT_ARG_STRING;
        } else if (strcmp(meta->type, "int") == 0) {
            flag = POPT_ARG_INT;
        } else if (strcmp(meta->type, "float") == 0) {
            flag = POPT_ARG_FLOAT;
        } else if (strcmp(meta->type, "bool") == 0) {
            flag = POPT_ARG_NONE;
        } else {
            assert(0);
        }
        set_option(options, i, meta->long_option, meta->short_option,
                   flag, meta->storage, 0,
                   meta->help, meta->help_arg);
        i++;
    }

    set_option(options, i, "version", 'v', POPT_ARG_NONE,
               NULL, 'v', "print version information and exit", NULL);
    i++;

    set_option(options, i, "dump-config", '\0', POPT_ARG_NONE,
               NULL, 'D', "dump configuration", NULL);
    i++;

    set_option(options, i, "config", 'c', POPT_ARG_STRING,
               NULL, 'c', "read FILE as a configuration file", "FILE");
    i++;

    set_option(options, i, NULL, '\0', POPT_ARG_INCLUDE_TABLE, 
               poptHelpOptions, 0, "Help options:", NULL);
    i++;

    set_option(options, i, NULL, '\0', 0, 0, 0, NULL, NULL);
    return options;
}

static void
set_config_value (MetaConfig *item, const char *key, const char *value,
                  const char *file_name, int lineno)
{
    char *err;
    if (item == NULL) {
        zphoto_eprintf("unknown parameter \"%s\" in %s:%d", 
                       key, file_name, lineno);
    } else if (strcmp(item->type, "int") == 0) {
        *(int *)item->storage = strtol(value, &err, 10);
        if (*err != '\0')
            zphoto_eprintf("invalid integer value \"%s\" in %s:%d", 
                           value, file_name, lineno);
    } else if (strcmp(item->type, "float") == 0) {
        *(float *)item->storage = strtod(value, &err);
        if (*err != '\0')
            zphoto_eprintf("invalid float value \"%s\" in %s:%d", 
                           value, file_name, lineno);
    } else if (strcmp(item->type, "string") == 0) {
        if (zphoto_strsuffixcasecmp(key, "_color") == 0 &&
            ! zphoto_valid_color_string_p(value)) 
        {
            zphoto_eprintf("invalid color value \"%s\" in %s:%d", 
                           value, file_name, lineno);
        }
        /*
         * FIXME: this string will never be freed.
         */
        *(char **)item->storage = zphoto_strdup(value);
    } else if (strcmp(item->type, "bool") == 0) {
        if (strcmp(value, "true") == 0) {
            *(int *)item->storage = 1;
        } else if (strcmp(value, "false") == 0) {
            *(int *)item->storage = 0;
        } else {
            assert(0);
        }
    } else {
        assert(0);
    }
}

static ZphotoConfig *
config_read (ZphotoConfig *config, const char* file_name)
{
    FILE* fp;
    char line[BUFSIZ];
    int lineno = 0;

    assert (file_name != NULL);
    fp = zphoto_efopen(file_name, "r");

    while (fgets(line, BUFSIZ, fp)) {
        char *sep, *key, *value;
        MetaConfig *item;
        lineno++;

        if (!zphoto_complete_line_p(line)) 
            zphoto_eprintf("too long line in %s:%d", file_name, lineno);
        zphoto_chomp(line);
        if (line[0] == '#' || zphoto_blank_line_p(line))
            continue;

        sep = line;
        sep += strcspn(sep, "\t =");
        if (*sep == '=') {  /* '=' found. ex. key=... */
            *sep = '\0';
        } else if (*sep == '\0') {
            zphoto_eprintf("syntax error in %s:%d", file_name, lineno);
        } else { /* ex. key =... */
            *sep = '\0';
            sep++; /* skip the first space */
            sep += strspn(sep, "\t "); /* skip preceding spaces */
            if (*sep != '=')
                zphoto_eprintf("syntax error in %s:%d", file_name, lineno);
        }
        sep++; /* skip = character */
        sep += strspn(sep, "\t "); /* skip following spaces */

        key = line;
        if (*sep == '\0')
            value = ""; /* empty value */
        else
            value = sep;
        item = meta_config_get(config->meta, key);
        set_config_value(item, key, value, file_name, lineno);
    }
    fclose(fp);
    return config;
}

static char *
tokenize (const char *str)
{
    char *new = zphoto_strdup(str);
    char *p = new;

    while (*p != '\0') {
        if (!isalnum(*p)) {
            *p = ' ';
        }
        p++;
    }
    return new;
}

static void
count_args (ZphotoConfig *config)
{
    char **p;
    config->args  = (char **)poptGetArgs(config->popcon);
    config->nargs = 0;
    for (p = config->args; p != NULL && *p != NULL; p++) {
        config->nargs++;
    }
}

static char *
rcfile_name (void)
{
    char *dir;
    char *rcfile_basename;

    if (zphoto_platform_w32_p()) {
        dir = ".";
        rcfile_basename = "zphoto.txt";
    } else {
        dir = getenv("HOME");
        if (dir == NULL)
            dir = ".";
        rcfile_basename = ".zphotorc";
    }
    return zphoto_asprintf("%s/%s", dir, rcfile_basename);
}

static char *
find_rcfile (void)
{
    char *file_name = rcfile_name();
    if (zphoto_path_exist_p(file_name)) {
        return file_name;
    } else {
        free(file_name);
        return NULL;
    }
}

void
zphoto_config_write (ZphotoConfig *config, FILE *out)
{
    MetaConfig *p;
    for (p = config->meta; p != NULL; p = p->next) {
        fprintf(out, "%-21s = ", p->key);
        if (strcmp(p->type, "int") == 0) {
            fprintf(out, "%d", *(int *)p->storage);
        } else if (strcmp(p->type, "float") == 0) {
            fprintf(out, "%f", *(float *)p->storage);
        } else if (strcmp(p->type, "string") == 0) {
            fprintf(out, "%s", *(char **)p->storage);
        } else if (strcmp(p->type, "bool") == 0) {
            if (*(int *)p->storage == 1) {
                fprintf(out, "true");
            } else if (*(int *)p->storage == 0) {
                fprintf(out, "false");
            } else {
                assert(0);
            }
        } else {
            assert(0);
        }
        fprintf(out, "\n");
    }
}

void
zphoto_config_save_rcfile (ZphotoConfig *config)
{
    FILE *fp;
    char *file_name = rcfile_name();
    
    fp = zphoto_efopen(file_name, "w");
    zphoto_config_write(config, fp);
    fclose(fp);
    free(file_name);
}

void
zphoto_config_parse (ZphotoConfig *config, int argc, char **argv)
{
    int option;
    struct poptOption *options = config_options(config);
    config->popcon = poptGetContext(argv[0], argc, (const char **)argv, 
                                    options,
                                    0);
    poptSetOtherOptionHelp(config->popcon, "[OPTION...] FILE...");
    poptResetContext(config->popcon);
    while ((option = poptGetNextOpt(config->popcon)) > 0) {
	switch (option) {
	case 'v': 
	    show_version();
	    exit(0);
	    break;
	case 'D': 
            zphoto_config_write(config, stdout);
            exit(0);
	    break;
	case 'c': 
            config_read(config, poptGetOptArg(config->popcon));
	    break;
	}

    }

    if (option < -1) {
	zphoto_eprintf("%s: %s", 
                       poptBadOption(config->popcon, POPT_BADOPTION_NOALIAS), 
                       poptStrerror(option));
    }

    if (strcmp(config->title, "") == 0)
        config->title = tokenize(zphoto_basename(config->output_dir));
    else
        config->title = zphoto_strdup(config->title);

    if (config->photo_width == 0 && config->include_original)
        config->include_original = 0;  /* disable it when no resizing */

    count_args(config);
}

ZphotoConfig *
zphoto_config_read_rcfile (ZphotoConfig *config)
{
    char *rcfile;

    rcfile = find_rcfile();
    if (rcfile) {
        return config_read(config, rcfile);
        free(rcfile);
    } else {
        return config;
    }
}

static void
reset_colors (ZphotoConfig *config, const char *file_name)
{
    char *colors_file = zphoto_asprintf("%s%c%s",
                                        config->template_dir,
                                        zphoto_path_separator(),
                                        file_name);
    if (zphoto_file_p(colors_file)) {
        config_read(config, colors_file);
    } else {
        zphoto_wprintf("%s is not found", colors_file);
    }
    free(colors_file);
}

void
zphoto_config_reset_flash_colors (ZphotoConfig *config)
{
    reset_colors(config, "flash-colors.txt");
}

void
zphoto_config_reset_css_colors (ZphotoConfig *config)
{
    reset_colors(config, "css-colors.txt");
}

void
zphoto_config_destroy (ZphotoConfig *config)
{
    free(config->title);
    free(config->template_dir);
    free(config->flash_font);
    meta_config_destroy(config->meta);
    poptFreeContext(config->popcon);
}

