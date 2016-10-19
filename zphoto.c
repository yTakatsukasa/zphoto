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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <popt.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zphoto.h>
#include <setjmp.h>
#include "config.h"

struct _Zphoto {
    ZphotoConfig *config;

    int         nphotos;
    char        **input_photos;
    char        **output_photos;
    char        **original_photos;
    time_t      *time_stamps;
    char        **thumbnails;
    char        **html_file_names;
    char        **photo_captions;
    char        **html_captions;

    ZphotoProgress *progress;
};

static void
make_flash (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    char *output_file_name = zphoto_asprintf("%s/%s",
					     config->output_dir,
					     config->flash_filename);
    ZphotoFlashMaker *maker = zphoto_flash_maker_new(
        zphoto->output_photos,
        zphoto->thumbnails, 
        zphoto->html_file_names,
        zphoto->photo_captions,
        zphoto->time_stamps,
        zphoto->nphotos,
        config->movie_nsamples,
        config->flash_width,
        config->flash_height,
        config->flash_font);
	
    if (config->art)
	zphoto_flash_maker_set_art_mode(maker);
    if (!config->no_fade)
	zphoto_flash_maker_set_fade_effect(maker);

    zphoto_flash_maker_set_caption_options(maker,config->disable_captions);
    zphoto_flash_maker_set_background_color(maker, 
                                            config->background_color);
    zphoto_flash_maker_set_photo_colors(maker, 
                                        config->border_active_color,
                                        config->border_inactive_color,
                                        config->shadow_color);
    zphoto_flash_maker_set_caption_colors(maker, 
                                          config->caption_border_color,
                                          config->caption_frame_color,
                                          config->caption_text_color);
    zphoto_flash_maker_set_progress_bar_colors(maker, 
                                               config->progress_bar_color,
                                               config->progress_bar_text_color,
                                               config->progress_bar_housing_color);

    zphoto_flash_maker_make(maker, output_file_name, zphoto->progress);
    zphoto_flash_maker_destroy(maker);
    free(output_file_name);
}

static void
make_thumbnails (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    int i;
    ZphotoImageCopier *copier = zphoto_image_copier_new();
    zphoto_progress_start(zphoto->progress, "thumbnail", 
                          N_("Creating thumbnails..."),
                          zphoto->nphotos);

    zphoto_image_copier_set_width(copier, config->thumbnail_width);
    if (config->gamma != 1.0)
	zphoto_image_copier_set_gamma(copier, config->gamma);

    for (i = 0; i < zphoto->nphotos; i++) {
	zphoto_progress_set(zphoto->progress, i,
                            zphoto_basename(zphoto->thumbnails[i]));
	zphoto_image_copier_copy(copier,
				 zphoto->input_photos[i],
				 zphoto->thumbnails[i],
				 zphoto->time_stamps[i]);
	
    }
    zphoto_progress_finish(zphoto->progress);
    zphoto_image_copier_destroy(copier);
}

static char *
int_to_str (int n)
{
    static char str[BUFSIZ];
    sprintf(str, "%d", n);
    return str;
}

static char *
concat (char *src, const char *dest)
{
    char *new = zphoto_asprintf("%s%s", src, dest);
    free(src);
    return new;
}

static char *
make_html_album (Zphoto *zphoto)
{
    int i;
    ZphotoConfig *config = zphoto->config;
    char *album = zphoto_strdup("<p class='thumbnails'>\n");

    for (i = 0; i < zphoto->nphotos; i++) {
        char *line;
        int width, height, thumbnail_height;
        char *html_url, *image_url;

        html_url = 
            zphoto_escape_url(zphoto_basename(zphoto->html_file_names[i]));
        image_url = 
            zphoto_escape_url(zphoto_basename(zphoto->thumbnails[i]));
        
        zphoto_image_get_size(zphoto->thumbnails[i], &width, &height);
        thumbnail_height = 
            height * ((double)config->html_thumbnail_width / width);

        line = zphoto_asprintf("<a href='%s'><img class='thumbnail' src='%s' width='%d' height='%d' alt='%s'></a>\n",
                               html_url,
                               image_url,
                               config->html_thumbnail_width,
                               thumbnail_height,
                               zphoto->photo_captions[i]);
	album = concat(album, line);
        free(html_url);
        free(image_url);
    }
    album = concat(album, "</p>\n");
    return album;
}		   
   

static void
add_common_substitutions (ZphotoConfig *config, 
                          ZphotoTemplate *template)
{
    time_t now = time(NULL);
    char *flash_file_name, *zip_file_name;

    flash_file_name = zphoto_escape_url(config->flash_filename);
    zip_file_name = zphoto_escape_url(config->zip_filename);

    zphoto_template_add_subst(template, "title", config->title);
    zphoto_template_add_subst(template, "date", ctime(&now));
    zphoto_template_add_subst(template, "flash_file_name", 
			      flash_file_name);
    zphoto_template_add_subst(template, "zip_file_name", 
			      zip_file_name);
    zphoto_template_add_subst(template, "flash_width", 
			      int_to_str(config->flash_width));
    zphoto_template_add_subst(template, "flash_height", 
			      int_to_str(config->flash_height));
    zphoto_template_add_subst(template, "zphoto_url", 
			      config->zphoto_url);
    if (zphoto_support_zip_p() && !config->no_zip) {
        zphoto_template_add_subst(template, "zip_link_start", "");
        zphoto_template_add_subst(template, "zip_link_end",   "");
    } else {
        zphoto_template_add_subst(template, "zip_link_start", "<!--");
        zphoto_template_add_subst(template, "zip_link_end",   "-->");
    }

    zphoto_template_add_subst(template, "css_background_color", 
                              config->css_background_color);
    zphoto_template_add_subst(template, "css_text_color", 
                              config->css_text_color);
    zphoto_template_add_subst(template, "css_footer_color", 
                              config->css_footer_color);
    zphoto_template_add_subst(template, "css_horizontal_line_color", 
                              config->css_horizontal_line_color);
    zphoto_template_add_subst(template, "css_photo_border_color", 
                              config->css_photo_border_color);
    zphoto_template_add_subst(template, "css_thumbnail_border_color", 
                              config->css_thumbnail_border_color);
    zphoto_template_add_subst(template, "css_navi_link_color", 
                              config->css_navi_link_color);
    zphoto_template_add_subst(template, "css_navi_visited_color", 
                              config->css_navi_visited_color);
    zphoto_template_add_subst(template, "css_navi_border_color", 
                              config->css_navi_border_color);
    zphoto_template_add_subst(template, "css_navi_hover_color", 
                              config->css_navi_hover_color);

    /*
     * It is safe to free them because template uses strdup
     * to hold key/value pairs.
     */
    free(flash_file_name);
    free(zip_file_name);
}

static void
add_index_substitutions (ZphotoConfig *config, 
                         ZphotoTemplate *template,
                         const char *html_album)
{
    add_common_substitutions(config, template);
    zphoto_template_add_subst(template, "album", html_album);
}

static char *
escape_html (const char *file_name)
{
    int max = sizeof("&quot;") - 1;
    char *new_file_name = zphoto_emalloc(strlen(file_name) * max + 1), *pp;
    const char *p;
    /*
     * It's safe to convert unsafe characters by
     * char-by-char fashion in Shift_JIS encoding because
     * they do not include the unsafe characters in the
     * second byte of the multi-byte characters.
     */
    for (p = file_name, pp = new_file_name; *p != '\0'; p++) {
        switch (*p) {
        case '<':
            strcpy(pp, "&lt;");
            break;
        case '>':
            strcpy(pp, "&gt;");
            break;
        case '&':
            strcpy(pp, "&amp;");
            break;
        case '"':
            strcpy(pp, "&quot;");
            break;
        default:
            pp[0] = *p;
            pp[1] = '\0';
        }
        pp += strlen(pp);
    }
    return new_file_name;
}

static void
add_photo_substitutions (Zphoto *zphoto, 
                         ZphotoTemplate *template,
                         int id)
{
    ZphotoConfig *config = zphoto->config;
    char *file_name, *thumbnail_file_name, *original_file_name, 
        *prev_html_file_name, *next_html_file_name, *next_file_name,
        *escaped_file_name;
    int w, h;
    char *width, *height;

    file_name = 
        zphoto_escape_url(zphoto_basename(zphoto->output_photos[id]));
    thumbnail_file_name = 
        zphoto_escape_url(zphoto_basename(zphoto->thumbnails[id]));
    original_file_name = 
        zphoto_escape_url(zphoto_basename(zphoto->original_photos[id]));
    escaped_file_name =
        escape_html(zphoto_basename(zphoto->output_photos[id]));

    if (id > 0) {
        prev_html_file_name = zphoto_basename(zphoto->html_file_names[id - 1]);
    } else {
        prev_html_file_name = 
            zphoto_basename(zphoto->html_file_names[zphoto->nphotos - 1]);
    }
    prev_html_file_name = zphoto_escape_url(prev_html_file_name);

    if (id < zphoto->nphotos - 1) {
        next_html_file_name = zphoto_basename(zphoto->html_file_names[id + 1]);
        next_file_name = zphoto_basename(zphoto->output_photos[id + 1]);
    } else {
        next_html_file_name = zphoto_basename(zphoto->html_file_names[0]);
        next_file_name = zphoto_basename(zphoto->output_photos[0]);
    }
    next_html_file_name = zphoto_escape_url(next_html_file_name);
    next_file_name = zphoto_escape_url(next_file_name);

    zphoto_image_get_size(zphoto->output_photos[id], &w, &h);
    width  = zphoto_asprintf("%d", w);
    height = zphoto_asprintf("%d", h);

    add_common_substitutions(config, template);

    if (zphoto_movie_file_p(file_name)) {
        zphoto_template_add_subst(template, "file_name", file_name);
        zphoto_template_add_subst(template, "thumbnail_file_name", 
                                  thumbnail_file_name);
        zphoto_template_add_subst(template, "photo_link_start", "<!--");
        zphoto_template_add_subst(template, "photo_link_end",   "-->");
        zphoto_template_add_subst(template, "movie_link_start", "");
        zphoto_template_add_subst(template, "movie_link_end",   "");
    } else {
        zphoto_template_add_subst(template, "file_name", file_name);
        if (config->include_original) {
            zphoto_template_add_subst(template, "original_file_name", 
                                      original_file_name);
        } else {
            zphoto_template_add_subst(template, "original_file_name", 
                                      file_name);
        }
        zphoto_template_add_subst(template, "photo_link_start", "");
        zphoto_template_add_subst(template, "photo_link_end",   "");
        zphoto_template_add_subst(template, "movie_link_start", "<!--");
        zphoto_template_add_subst(template, "movie_link_end",   "-->");
    }

    zphoto_template_add_subst(template, "width",  width);
    zphoto_template_add_subst(template, "height", height);
    zphoto_template_add_subst(template, "time", 
                              zphoto_time_string(zphoto->time_stamps[id]));

    zphoto_template_add_subst(template, "prev_html_file_name", 
                              prev_html_file_name);
    zphoto_template_add_subst(template, "next_html_file_name", 
                              next_html_file_name);
    zphoto_template_add_subst(template, "next_file_name", 
                              next_file_name);
    if (zphoto->html_captions[id] != NULL) {
        zphoto_template_add_subst(template, "caption", 
                                  zphoto->html_captions[id]);
        zphoto_template_add_subst(template, "caption_start", "");
        zphoto_template_add_subst(template, "caption_end", "");
    } else {
        zphoto_template_add_subst(template, "caption", "");
        zphoto_template_add_subst(template, "caption_start", "<!--");
        zphoto_template_add_subst(template, "caption_end", "-->");
    }

    zphoto_template_add_subst(template, "escaped_file_name", 
                              escaped_file_name);

    free(file_name);
    free(thumbnail_file_name);
    free(original_file_name);
    free(prev_html_file_name);
    free(next_html_file_name);
    free(next_file_name);
    free(escaped_file_name);
    free(width);
    free(height);
}

static void
make_photo_html_files (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    int i;
    char *template_file_name = zphoto_asprintf("%s/.photo.html", 
                                               config->template_dir);

    zphoto_progress_start(zphoto->progress, "html", 
                          N_("Creating HTML files..."),
                          zphoto->nphotos);
    for (i = 0; i < zphoto->nphotos; i++) {
        ZphotoTemplate *template;
	zphoto_progress_set(zphoto->progress, i, 
                            zphoto_basename(zphoto->html_file_names[i]));

        template = zphoto_template_new(template_file_name);
        add_photo_substitutions(zphoto, template, i);
        zphoto_template_write(template, zphoto->html_file_names[i]);
        zphoto_template_destroy(template);
    }
    free(template_file_name);
    zphoto_progress_finish(zphoto->progress);
}

static void
make_index_html_files (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    char *html_album;
    DIR  *template_dir;
    struct dirent *d;

    if (config->template_dir == NULL)
	return;

    html_album = make_html_album(zphoto);
    template_dir = zphoto_eopendir(config->template_dir);
    while ((d = readdir(template_dir))) {
        char *d_name = zphoto_d_name_workaround(d);
	char *file_name = zphoto_asprintf("%s/%s", 
					  config->template_dir,
					  d_name);
	if (zphoto_web_file_p(file_name) && !zphoto_dot_file_p(d_name)) {
            char *output_file_name = zphoto_asprintf("%s/%s", 
                                                     config->output_dir,
                                                     d_name);
            ZphotoTemplate *template = zphoto_template_new(file_name);
            add_index_substitutions(config, template, html_album);

            zphoto_template_write(template, output_file_name);
            zphoto_template_destroy(template);
            free(output_file_name);
	}
	free(file_name);
    }
    closedir(template_dir);
    free(html_album);
}

/*
 * FIXME: The function probably doesn't work on the Windows platform.
 */
static char *
escape_unix (const char *file_name)
{
    char *new_file_name = zphoto_emalloc(strlen(file_name) * 2 + 1), *pp;
    const char *p;
    for (p = file_name, pp = new_file_name; *p != '\0'; p++, pp++) {
        if (strchr("$`\"\\", *p) != NULL) {
            *pp++ = '\\';
        }
        *pp = *p;
    }
    *pp = '\0';
    return new_file_name;
}

static void
make_zip_file (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    int i;
    char *tmp, *output_zip_file_name, *zip_command;
    zphoto_progress_start(zphoto->progress, "zip", 
                          N_("Creating a zip file..."),
                          zphoto->nphotos);

    zip_command = escape_unix(config->zip_command);
    tmp = zphoto_asprintf("%s/%s", config->output_dir, config->zip_filename);
    output_zip_file_name  = escape_unix(tmp);
    free(tmp);

    for (i = 0; i < zphoto->nphotos; i++) {
        char *photo_file_name = escape_unix(zphoto->original_photos[i]);
        char *command = zphoto_asprintf("%s \"%s\" \"%s\"",
                                        zip_command,
                                        output_zip_file_name,
                                        photo_file_name);

	zphoto_progress_set(zphoto->progress, i,
                            zphoto_basename(zphoto->input_photos[i]));
        system(command);
        free(command);
        free(photo_file_name);
    }
    zphoto_progress_finish(zphoto->progress);
    free(output_zip_file_name);
    free(zip_command);
}

static void
copy_photos (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    int i;
    ZphotoImageCopier *copier = zphoto_image_copier_new();
    char *task = "copy";
    char *task_long = N_("Copying images...");

    if (config->photo_width > 0) {
	zphoto_image_copier_set_width(copier, config->photo_width);
        task = "resize";
        task_long = N_("Resizing images...");
    }
    if (config->gamma != 1.0)
	zphoto_image_copier_set_gamma(copier, config->gamma);

    zphoto_progress_start(zphoto->progress, task, task_long, zphoto->nphotos);
    for (i = 0; i < zphoto->nphotos; i++) {
	zphoto_progress_set(zphoto->progress, i, 
                            zphoto_basename(zphoto->output_photos[i]));
	zphoto_image_copier_copy(copier,
				 zphoto->input_photos[i],
				 zphoto->output_photos[i],
                                 zphoto->time_stamps[i]);
    }
    zphoto_progress_finish(zphoto->progress);
    zphoto_image_copier_destroy(copier);
}

static void
include_photos (Zphoto *zphoto)
{
    int i;
    ZphotoImageCopier *copier = zphoto_image_copier_new();

    zphoto_progress_start(zphoto->progress, "include",
                          N_("Including originals..."),
                          zphoto->nphotos);
    for (i = 0; i < zphoto->nphotos; i++) {
	zphoto_progress_set(zphoto->progress, i, 
                            zphoto_basename(zphoto->input_photos[i]));
	zphoto_image_copier_copy(copier,
				 zphoto->input_photos[i],
				 zphoto->original_photos[i],
                                 zphoto->time_stamps[i]);
    }
    zphoto_progress_finish(zphoto->progress);
    zphoto_image_copier_destroy(copier);
}

static void
progress_bar (ZphotoProgress *progress)
{
    char bar[]  = "oooooooooooooooooooooooooooooooooo";
    int scale = sizeof(bar) - 1;
    int bar_len   = (double)progress->current * scale / progress->total;

    fprintf(stderr, "%-10s |%.*s%*s| %-20s [%d/%d]\r", 
	    progress->task,
	    bar_len, bar, scale - bar_len, "", 
	    progress->file_name, 
	    progress->current, progress->total);
    if (progress->is_finished)
	fprintf(stderr, "\n");
    else
	fprintf(stderr, "\r");
    fflush(stderr);
}

Zphoto *
zphoto_new (ZphotoConfig *config)
{
    Zphoto *zphoto;
    zphoto = zphoto_emalloc(sizeof(Zphoto));
    zphoto->config  = config;

    zphoto->nphotos = 0;
    zphoto->input_photos = NULL;
    zphoto->output_photos = NULL;
    zphoto->original_photos = NULL;
    zphoto->thumbnails = NULL;
    zphoto->html_file_names = NULL;
    zphoto->photo_captions = NULL;
    zphoto->html_captions = NULL;

    zphoto->progress = zphoto_progress_new();
    if (!config->quiet)
        zphoto_progress_set_func(zphoto->progress, progress_bar);

    return zphoto;
}

static void
swap (Zphoto *zphoto, int a, int b)
{
    char  *tmp_name = zphoto->input_photos[a];
    time_t tmp_time = zphoto->time_stamps[a];

    zphoto->input_photos[a] = zphoto->input_photos[b];
    zphoto->input_photos[b] = tmp_name;

    zphoto->time_stamps[a] = zphoto->time_stamps[b];
    zphoto->time_stamps[b] = tmp_time;
}

static void
sort_by_filename (Zphoto *zphoto)
{
    int i, j;
    for (i = 0; i < zphoto->nphotos; i++) {
        for (j = i + 1; j < zphoto->nphotos; j++) {
            if (strcmp(zphoto->input_photos[j], 
                       zphoto->input_photos[i]) < 0) {
                swap(zphoto, i, j);
            }
        }
    }
}

static void
sort_by_time (Zphoto *zphoto)
{
    int i, j;
    for (i = 0; i < zphoto->nphotos; i++) {
        for (j = i + 1; j < zphoto->nphotos; j++) {
            if (zphoto->time_stamps[j] < zphoto->time_stamps[i]) {
                swap(zphoto, i, j);
            }
        }
    }
}

static int
same_files_p (const char *file_name1, const char *file_name2)
{
    struct stat st1, st2;

    stat(file_name1, &st1);
    stat(file_name2, &st2);

    if (zphoto_platform_w32_p()) {
         /* 
          * FIXME: Not supported yet 
          */
        return 0; 
    } else {
        /*
         * FIXME: Use realpath?
         */
        if (st1.st_ino == st2.st_ino)
            return 1;
        else
            return 0;
    }
}

static void
set_file_names (Zphoto *zphoto, int i)
{
    ZphotoConfig *config = zphoto->config;
    char *output_file_name, *thumbnail_file_name, *html_file_name,
        *original_file_name, *base, *nosuffix, *suffix, *preview_prefix;

    base = zphoto_basename(zphoto->input_photos[i]);
    nosuffix = zphoto_suppress_suffix(zphoto_strdup(base));
    suffix   = zphoto_get_suffix(base);
    
    if (config->include_original)
        preview_prefix = config->preview_prefix;
    else 
        preview_prefix = "";

    if (config->sequential) {
        output_file_name = zphoto_asprintf("%s/%s%06d.%s", 
                                           config->output_dir, 
                                           preview_prefix,
                                           i + 1,
                                           suffix);
        original_file_name = zphoto_asprintf("%s/%06d.%s", 
                                             config->output_dir, 
                                             i + 1, 
                                             suffix);
        thumbnail_file_name = zphoto_asprintf("%s/%s%06d.jpg",
                                              config->output_dir,
                                              config->thumbnail_prefix,
                                              i + 1);
        html_file_name = zphoto_asprintf("%s/%06d.html%s",
                                         config->output_dir,
                                         i + 1,
                                         config->html_suffix);
    } else {
        output_file_name = zphoto_asprintf("%s/%s%s",
                                           config->output_dir,
                                           preview_prefix,
                                           base);
        original_file_name = zphoto_asprintf("%s/%s",
                                             config->output_dir,
                                             base);
        thumbnail_file_name = zphoto_asprintf("%s/%s%s.jpg",
                                              config->output_dir,
                                              config->thumbnail_prefix,
                                              nosuffix);
        html_file_name = zphoto_asprintf("%s/%s.html%s",
                                         config->output_dir,
                                         nosuffix,
                                         config->html_suffix);
    }
    free(nosuffix);

    zphoto->output_photos[i] = output_file_name;
    zphoto->thumbnails[i]    = thumbnail_file_name;
    zphoto->html_file_names[i] = html_file_name;
    zphoto->original_photos[i] = original_file_name;

    if (same_files_p(zphoto->input_photos[i],
                     zphoto->output_photos[i])) {
        zphoto_eprintf("input and output file names are same: %s", 
                       output_file_name);
    }
}

static void
set_caption (Zphoto *zphoto, ZphotoAlist *caption_table, int i)
{
    ZphotoConfig *config = zphoto->config;
    char *caption = NULL;
    char *defined_caption = 
        zphoto_alist_get(caption_table, 
                         zphoto_basename(zphoto->input_photos[i]));

    if (defined_caption) {
        caption = zphoto_strdup(defined_caption);
    } else if (config->caption_by_filename) {
        caption = zphoto_strdup(zphoto_basename(zphoto->input_photos[i]));
    } else {
        caption = zphoto_strdup(zphoto_time_string(zphoto->time_stamps[i]));
    }

    zphoto->photo_captions[i] = caption;
    if (defined_caption)
        zphoto->html_captions[i] = zphoto_strdup(defined_caption);
    else
        zphoto->html_captions[i] = NULL;
}

static ZphotoAlist *
read_caption_table (const char* file_name)
{
    FILE* fp;
    char line[BUFSIZ];
    ZphotoAlist *alist = NULL;
    int lineno = 0;

    assert (file_name != NULL);
    fp = zphoto_efopen(file_name, "r");

    while (fgets(line, BUFSIZ, fp)) {
        char *sep, *photo_file_name, *caption;
        lineno++;

        if (!zphoto_complete_line_p(line)) 
            zphoto_eprintf("too long line in %s:%d", file_name, lineno);
        zphoto_chomp(line);

        if (line[0] == '#' || zphoto_blank_line_p(line))
            continue;
        if ((sep = strchr(line, '\t'))) {
            *sep = '\0';
            sep++;
            while (*sep == '\t') sep++;

            photo_file_name = zphoto_basename(line);
            caption = sep;
            alist = zphoto_alist_add(alist, photo_file_name, caption);
        }
    }
    fclose(fp);
    return alist;
}


void
zphoto_add_file_names (Zphoto *zphoto, char **file_names, int nfile_names)
{
    ZphotoConfig *config = zphoto->config;
    ZphotoAlist *caption_table = NULL;
    int i, j;

    /*
     * FIXME: repeated call is not supported yet.
     */
    assert(zphoto->input_photos == NULL);

    zphoto->input_photos = zphoto_emalloc(sizeof(char *) * nfile_names);
    for (i = j = 0; i < nfile_names ; i++) {
        if (zphoto_supported_file_p(file_names[i])) {
            zphoto->input_photos[j] = zphoto_strdup(file_names[i]);
            j++;
        } else {
            zphoto_wprintf("%s: not a supported file", file_names[i]);
        }
    }
    zphoto->nphotos = j;

    zphoto->output_photos   = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);
    zphoto->original_photos = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);
    zphoto->thumbnails      = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);
    zphoto->html_captions   = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);
    zphoto->time_stamps     = zphoto_emalloc(sizeof(time_t) * zphoto->nphotos);
    zphoto->html_file_names = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);
    zphoto->photo_captions  = zphoto_emalloc(sizeof(char *) * zphoto->nphotos);

    for (i = 0; i < zphoto->nphotos; i++) {
        time_t time = zphoto_image_get_time(zphoto->input_photos[i], 
                                            config->no_exif);
	zphoto->time_stamps[i] = time;
    }

    if (config->sort_by_filename) {
        sort_by_filename(zphoto);
    } else if (!config->no_sort) {
        sort_by_time(zphoto);
    }

    if (strcmp(config->caption_file, "") != 0)
        caption_table = read_caption_table(config->caption_file);

    for (i = 0; i < zphoto->nphotos; i++) {
        set_file_names(zphoto, i);
        set_caption(zphoto, caption_table, i);
    }

    zphoto_alist_destroy(caption_table);
}

void
zphoto_destroy (Zphoto *zphoto)
{
    int i;
    assert(zphoto->thumbnails);
    assert(zphoto->input_photos);
    assert(zphoto->output_photos);
    assert(zphoto->original_photos);
    assert(zphoto->time_stamps);
    assert(zphoto->html_file_names);
    assert(zphoto->photo_captions);
    assert(zphoto->html_captions);

    zphoto_progress_destroy(zphoto->progress);

    for (i = 0; i < zphoto->nphotos; i++) {
	free(zphoto->input_photos[i]);
	free(zphoto->output_photos[i]);
	free(zphoto->original_photos[i]);
	free(zphoto->thumbnails[i]);
	free(zphoto->html_file_names[i]);
	free(zphoto->photo_captions[i]);
        if (zphoto->html_captions[i])
            free(zphoto->html_captions[i]);
    }

    free(zphoto->input_photos);
    free(zphoto->output_photos);
    free(zphoto->thumbnails);
    free(zphoto->time_stamps);
    free(zphoto->html_file_names);
    free(zphoto->photo_captions);
    free(zphoto->html_captions);

    free(zphoto);
}

static int
create_zip_file_p (ZphotoConfig *config)
{
    return zphoto_support_zip_p() &&  !config->no_zip;
}

int
zphoto_get_nsteps (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;

    int nsteps = 0;
    nsteps++;   /* copy/resize  */
    if (config->include_original)
        nsteps++;
    nsteps++;   /* thumbnail  */
    nsteps++;   /* flash */
    nsteps++;   /* html */
    if (create_zip_file_p(config)) 
        nsteps++;

    return nsteps;
}

void
zphoto_make_all (Zphoto *zphoto)
{
    ZphotoConfig *config = zphoto->config;
    assert(zphoto->input_photos != NULL);
    if (zphoto->nphotos == 0)
        return;

    if (setjmp(zphoto->progress->jmpbuf) == 0) {
        zphoto_mkdir(config->output_dir);
        copy_photos(zphoto);
        if (config->include_original)
            include_photos(zphoto);
        make_thumbnails(zphoto);
        make_flash(zphoto);
        make_photo_html_files(zphoto);
        make_index_html_files(zphoto);
        if (create_zip_file_p(config))
            make_zip_file(zphoto);
    }
}

void
zphoto_set_progress (Zphoto *zphoto, ZphotoProgressFunc func, void *data)
{
    zphoto_progress_set_func(zphoto->progress, func);
    zphoto_progress_set_data(zphoto->progress, data);
}

void
zphoto_abort (Zphoto *zphoto)
{
    zphoto_progress_abort(zphoto->progress);
}


