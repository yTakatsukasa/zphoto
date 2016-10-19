/* 
 * Zphoto - a zooming photo album generator.
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ming.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <zphoto.h>
#include <ctype.h>
#include "config.h"

typedef struct {
    float width;
    float height;
} Size;

typedef struct {
    SWFText text;
    Size size;
} Text;

typedef struct {
    SWFShape shape;
    Size size;
} CaptionFrame;

typedef struct {
    SWFMovieClip clip;
    Size size;
} Caption;

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    byte r;
    byte g;
    byte b;
    byte a;
} Color;

typedef struct {
    char	*full_size_file_name;
    char	*thumbnail_file_name;
    char	*html_file_name;
    char	*caption;
    time_t	time;
    FILE	*file;
    SWFBitmap	bitmap;
    Size	size;
    int		id;
    SWFBitmap	*sample_bitmaps;
    FILE	**sample_files;
    char	**sample_file_names;
    int		nsamples;
    int		loaded_p;
} Photo;

typedef Point		(*CalcZoomingOffsetUnitFunc) (ZphotoFlashMaker *maker,
						      Size photo_size);

typedef Point		(*CalcOriginFunc)	     (ZphotoFlashMaker *maker);
typedef SWFMovieClip	(*CreateZoomingClipFunc)     (ZphotoFlashMaker *maker, 
						      Photo *photo, 
						      int x,
						      int y);
typedef void		(*CreateZoomingFrameFunc)    (ZphotoFlashMaker *maker, 
						      SWFDisplayItem item,
						      Photo *photo,
						      int i);
typedef void		(*AddTransitionFunc)	     (ZphotoFlashMaker *maker,
                                                      SWFMovieClip clip,
                                                      Photo *photo,
                                                      unsigned short 
                                                      border_line_width,
                                                      Color border_color);

struct _ZphotoFlashMaker {
    int   x_nphotos;
    int   y_nphotos;
    float flash_rate;
    float photo_x_unit;
    float photo_y_unit;
    float zooming_scale;
    float photo_margin;
    float album_margin;
    float caption_height;
    float caption_margin;
    float caption_offset;
    float caption_width_ratio;
    float progress_bar_text_height;
    float bottom_margin;
    Size  flash_size;
    Size  canvas_size;
    Size  progress_bar_size;

    unsigned short progress_bar_housing_line_width;
    unsigned short border_active_line_width;
    unsigned short border_inactive_line_width;
    unsigned short caption_border_line_width;
    int zooming_nframes;
    int wait_nframes;
    int invisible_nframes;

    int disable_captions;

    char *flash_font_name;

    Photo **photos;
    int nphotos;

    int nsamples;
    int transition_nframes;

    SWFSound mouse_over_sound;

    Color background_color;

    Color border_active_color;
    Color border_inactive_color;

    Color shadow_color;

    Color caption_border_color;
    Color caption_frame_color;
    Color caption_text_color;
    Color progress_bar_text_color;
    Color progress_bar_color;
    Color progress_bar_housing_color;

    CreateZoomingClipFunc  create_zooming_clip_func;
    CreateZoomingFrameFunc create_zooming_in_frame_func;
    CreateZoomingFrameFunc create_zooming_out_frame_func;
    AddTransitionFunc add_transition_func;
};

static void
draw_rectangle (SWFShape shape, Size size)
{
    SWFShape_movePenTo(shape, 0, 0); 
    SWFShape_drawLine(shape, size.width, 0); 
    SWFShape_drawLine(shape, 0, size.height);
    SWFShape_drawLine(shape, -size.width, 0); 
    SWFShape_drawLine(shape, 0, -size.height);
}

static SWFShape
create_image (ZphotoFlashMaker *maker, 
	      SWFBitmap bitmap,
              Size size,
	      unsigned short border_line_width,
	      Color border_color)
{
    SWFFill   fill;
    SWFShape shape;

    shape = newSWFShape();
    fill = SWFShape_addBitmapFill(shape, bitmap, SWFFILL_BITMAP);
    SWFShape_setRightFill(shape, fill);
    SWFShape_setLineStyle(shape, border_line_width, 
			  border_color.r,
			  border_color.g,
			  border_color.b,
			  border_color.a);
    draw_rectangle(shape, size);

    return shape;
}

static Color
create_color (byte r, byte g, byte b, byte a)
{
    Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    return color;
}

static Color
translate_color (const char *input_color)
{
    int r, g, b, a;
    zphoto_decode_color_string(input_color, &r, &g, &b, &a);
    return create_color(r, g, b, a);
}

static void
get_samples (Photo *photo, int nsamples)
{
    int i;

    photo->sample_file_names = 
        zphoto_image_save_samples(photo->full_size_file_name, 
                                  photo->thumbnail_file_name,
                                  nsamples);

    for (i = 0; photo->sample_file_names[i] != NULL; i++); 
    nsamples = i;
    photo->sample_files = zphoto_emalloc(sizeof(FILE*) * nsamples);
    photo->sample_bitmaps = zphoto_emalloc(sizeof(SWFBitmap) * nsamples);

    for (i = 0; i < nsamples; i++) {
        photo->sample_files[i]   = zphoto_efopen(photo->sample_file_names[i],
                                                 "r");
        photo->sample_bitmaps[i] = newSWFJpegBitmap(photo->sample_files[i]);
    }
    photo->nsamples = i;
}

static void 
load_photo (ZphotoFlashMaker *maker, Photo *photo)
{
    assert(photo->loaded_p == 0);

    photo->file  = zphoto_efopen(photo->thumbnail_file_name, "rb");
    photo->bitmap = newSWFJpegBitmap(photo->file);
    photo->size.width  = SWFBitmap_getWidth(photo->bitmap);
    photo->size.height = SWFBitmap_getHeight(photo->bitmap);


    if (zphoto_movie_file_p(photo->full_size_file_name) &&
        zphoto_support_movie_p())
    {
        get_samples(photo, maker->nsamples);
        if (photo->nsamples < 1)
            zphoto_eprintf("broken movie file: %s",
                           photo->full_size_file_name);
    }
    photo->loaded_p = 1;
}

static Photo *
create_photo (const char *full_size_file_name, 
              const char *thumbnail_file_name,
              const char *html_file_name,
              const char *caption,
              time_t time,
              int id)
{
    Photo *photo = zphoto_emalloc(sizeof(Photo));
    photo->full_size_file_name  = zphoto_strdup(full_size_file_name);
    photo->caption = zphoto_strdup(caption);
    photo->html_file_name = zphoto_strdup(html_file_name);
    photo->thumbnail_file_name  = zphoto_strdup(thumbnail_file_name);
    photo->time  = time;
    photo->id = id;
    photo->sample_files = NULL;
    photo->sample_file_names = NULL;
    photo->sample_bitmaps = NULL;
    photo->nsamples = 0;
    photo->loaded_p = 0;
    return photo;
}

static void
destroy_photo (Photo *photo)
{
    free(photo->full_size_file_name);
    free(photo->thumbnail_file_name);
    free(photo->html_file_name);
    free(photo->caption);

    /*
     * FIXME: It's too bad those files should be closed
     * after calling SWFMovie_setRate(). Otherwise, we get
     * fatal errors such as a bus error...
     */
    fclose(photo->file);
    if (photo->nsamples > 0) {
        int i;
        for (i = 0; i < photo->nsamples; i++) {
            fclose(photo->sample_files[i]);
            remove(photo->sample_file_names[i]);
        }
        free(photo->sample_files);
        free(photo->sample_file_names);
    }
    free(photo);
}


static Photo **
collect_photos (char **full_size_file_names, 
		char **thumbnail_file_names, 
		char **html_file_names,
		char **captions,
		time_t *time_stamps, 
		int nphotos)
{
    int i;
    Photo **photos = zphoto_emalloc(sizeof(Photo *) * nphotos);
	
    for (i = 0; i < nphotos; i++)
    {
        photos[i] = create_photo(
            full_size_file_names[i],
            thumbnail_file_names[i],
            html_file_names[i],
            captions[i],
            time_stamps[i], 
            i);
    }
    return photos;
}

static SWFShape
create_shadow (ZphotoFlashMaker *maker, Size size)
{
    SWFShape shape = newSWFShape();
    SWFFill  fill  = SWFShape_addSolidFill(shape, 
					   maker->shadow_color.r,
					   maker->shadow_color.g,
					   maker->shadow_color.b,
					   maker->shadow_color.a); 

    SWFShape_setRightFill(shape, fill);
    draw_rectangle(shape, size);
    return shape;
}

static void
add_wait_frames (ZphotoFlashMaker *maker, SWFMovieClip clip, int nframes)
{
    int i;
    for (i = 0; i < nframes; i++)
	SWFMovieClip_nextFrame(clip);
}

static void
add_transition (ZphotoFlashMaker *maker,
                SWFMovieClip clip,
                Photo *photo,
                unsigned short border_line_width,
                Color border_color)
{
    int i;
    for (i = 0; i < photo->nsamples; i++) {
        SWFShape current = create_image(maker, 
                                        photo->sample_bitmaps[i], photo->size,
                                        border_line_width, border_color);
        SWFMovieClip_add(clip, current);
        add_wait_frames(maker, clip, maker->transition_nframes);
    }
}

static void
add_transition_with_fade_effect (ZphotoFlashMaker *maker,
                                 SWFMovieClip clip,
                                 Photo *photo,
                                 unsigned short border_line_width,
                                 Color border_color)
{
    int i;
    for (i = 0; i < photo->nsamples; i++) {
        int j;
        SWFShape current, next;
        SWFDisplayItem fore;

        current = create_image(maker, 
                               photo->sample_bitmaps[i], photo->size,
                               border_line_width, border_color);
        next = create_image(maker, 
                            photo->sample_bitmaps[(i + 1) % photo->nsamples],
                            photo->size,
                            border_line_width, border_color);
        SWFMovieClip_add(clip, next);
        fore = SWFMovieClip_add(clip, current);
        for (j = 1; j <= maker->transition_nframes; j++) {
            SWFDisplayItem_setColorMult(fore, 1.0, 1.0, 1.0, 
                                        1.0 - (float)j / 
                                        maker->transition_nframes);
            SWFMovieClip_nextFrame(clip);
        }
    }
}

static SWFMovieClip
create_shadowed_photo (ZphotoFlashMaker *maker,
		       Photo *photo,
		       unsigned short border_line_width,
		       Color border_color)
{
    SWFDisplayItem shadow;
    SWFMovieClip clip;
    float shadow_offset;

    clip  = newSWFMovieClip();
    shadow_offset = photo->size.width  * maker->photo_margin / 2;

    shadow = SWFMovieClip_add(clip, create_shadow(maker, photo->size));
    SWFDisplayItem_move(shadow, shadow_offset, shadow_offset);
    if (zphoto_movie_file_p(photo->full_size_file_name)) {
        maker->add_transition_func(maker, clip, photo, 
                                   border_line_width, border_color);
    } else {
        SWFShape image = create_image(maker, photo->bitmap, photo->size,
                                      border_line_width, border_color);
        SWFMovieClip_add(clip, image);
        SWFMovieClip_nextFrame(clip);
    }

    return clip;
}

static SWFMovieClip
create_shadowed_photo_inactive (ZphotoFlashMaker *maker, Photo *photo)
{
    return create_shadowed_photo(maker, 
                                 photo, 
                                 maker->border_inactive_line_width,
                                 maker->border_inactive_color);
}


static SWFMovieClip
create_shadowed_photo_active (ZphotoFlashMaker *maker, Photo *photo)
{
    return create_shadowed_photo(maker, 
                                 photo, 
                                 maker->border_active_line_width,
                                 maker->border_active_color);
}


static Text
create_text (ZphotoFlashMaker *maker, 
	     const char *str, 
	     float height,
	     Color color)
{
    Text text;
    FILE *fp = zphoto_efopen(maker->flash_font_name, "rb");
    SWFFont font = loadSWFFontFromFile(fp);
	
    text.text = newSWFText2();
    SWFText_setFont(text.text, font);
    SWFText_setColor(text.text, color.r, color.g, color.b, color.a);
    SWFText_setHeight(text.text, height);
    SWFText_addString(text.text, str,NULL);

    text.size.width  = SWFText_getStringWidth(text.text, str);
    text.size.height = SWFText_getAscent(text.text);
    fclose(fp);

    return text;
}

static CaptionFrame
create_caption_frame (ZphotoFlashMaker *maker, Text text)
{
    CaptionFrame frame;
    SWFFill fill;

    frame.size.width  = text.size.width + maker->caption_offset * 2;
    frame.size.height = maker->caption_height;

    frame.shape = newSWFShape();
    SWFShape_setLineStyle(frame.shape, maker->caption_border_line_width, 
			  maker->caption_border_color.r,
			  maker->caption_border_color.g,
			  maker->caption_border_color.b,
			  maker->caption_border_color.a);
    fill  = SWFShape_addSolidFill(frame.shape,
				  maker->caption_frame_color.r,
				  maker->caption_frame_color.g,
				  maker->caption_frame_color.b,
				  maker->caption_frame_color.a);
    SWFShape_setRightFill(frame.shape, fill);
    draw_rectangle(frame.shape, frame.size);

    return frame;
}

static float
calc_caption_height (ZphotoFlashMaker *maker)
{
    return maker->caption_height - maker->caption_offset * 2;
}

static Caption
create_caption (ZphotoFlashMaker *maker, const char *message)
{
    Caption caption;
    Text  text;
    CaptionFrame frame;
    SWFDisplayItem item;

    text  = create_text(maker, message, 
			calc_caption_height(maker), maker->caption_text_color);
    frame = create_caption_frame(maker, text);

    caption.clip = newSWFMovieClip();
    caption.size = frame.size;

    SWFMovieClip_add(caption.clip, frame.shape);

    item = SWFMovieClip_add(caption.clip, text.text);
    SWFDisplayItem_moveTo(item, 0, frame.size.height);
    SWFDisplayItem_move(item, 
			maker->caption_offset, - maker->caption_offset);

    SWFMovieClip_nextFrame(caption.clip);
    return caption;
}


static float
calc_caption_scale (ZphotoFlashMaker *maker, 
		    Size photo_size, 
		    Size caption_size,
		    float photo_scale)
{
    return photo_size.width / caption_size.width * photo_scale * 
	maker->caption_width_ratio;
}

static Point
calc_caption_point (ZphotoFlashMaker *maker, 
		    Size photo_size,
		    Size caption_size, 
		    float caption_scale,
		    float photo_scale,
		    Point photo_offset)
{
    Point point;

    point.x = photo_offset.x + photo_size.width  * photo_scale - 
	caption_size.width * caption_scale;

    point.y = photo_offset.y + photo_size.height * photo_scale;

    return point;
}

static void
add_caption (ZphotoFlashMaker *maker, 
	     SWFMovieClip clip,
	     const char *message,
	     Size photo_size,
	     float scale,
	     Point offset)
{
    SWFDisplayItem item;
    Caption caption;
    float caption_scale;
    Point caption_point;

    caption = create_caption(maker, message);
    caption_scale = calc_caption_scale(maker, photo_size, 
				       caption.size, scale);
    caption_point = calc_caption_point(maker, 
				       photo_size,
				       caption.size, 
				       caption_scale, 
				       scale, offset);

    item = SWFMovieClip_add(clip, caption.clip);
    SWFDisplayItem_setName(item, "caption");
    SWFDisplayItem_moveTo(item, caption_point.x, caption_point.y);
    SWFDisplayItem_scaleTo(item, caption_scale, caption_scale);

    SWFMovieClip_nextFrame(clip);
}

static Point
calc_origin (ZphotoFlashMaker *maker)
{
    Point origin;

    origin.x = (maker->flash_size.width  - maker->canvas_size.width)  / 2;
    origin.y = (maker->flash_size.height - maker->canvas_size.height) / 2 -
        maker->bottom_margin;
    return origin;
}

static float
calc_zooming_scale (ZphotoFlashMaker *maker, int i)
{
    float x = 1.0 - pow(2.0, - i);
    return 1.0 + x * (maker->zooming_scale - 1.0);
}

static Point
calc_zooming_offset (ZphotoFlashMaker *maker, Size size, int i)
{
    Point zooming_offset;
    float x = 1.0 - pow(2.0, - i);
    float w = size.width  * (maker->zooming_scale - 1.0) / 2;
    float h = size.height * (maker->zooming_scale - 1.0) / 2;

    zooming_offset.x = - w * x;
    zooming_offset.y = - h * x;
    return zooming_offset;
}

static void
add_starting_frames (ZphotoFlashMaker *maker, 
		     SWFMovieClip clip,
		     SWFDisplayItem item,
                     Photo *photo)
{
    int i;

    SWFMovieClip_add(clip, compileSWFActionCode(
			 "if (loaded) gotoAndPlay('loaded');"));
    
    for (i = 1; i <= maker->zooming_nframes; i++) {
	float scale = 2 - i / (float)maker->zooming_nframes;
	SWFDisplayItem_setColorMult(item, 1, 1, 1, 
				    (float)i / maker->zooming_nframes);
	SWFDisplayItem_scaleTo(item, scale, scale);
	SWFMovieClip_nextFrame(clip);
    }

    SWFMovieClip_labelFrame(clip, "loaded");
    SWFMovieClip_add(clip, compileSWFActionCode("stop(); loaded = 1;"));
    SWFMovieClip_nextFrame(clip);

    SWFMovieClip_labelFrame(clip, "zoom_in");
    SWFMovieClip_add(clip, compileSWFActionCode("nframes = 0;"));
    SWFMovieClip_nextFrame(clip);
}

static SWFAction
create_open_action (Photo *photo)
{
    char *url = zphoto_escape_url(zphoto_basename(photo->html_file_name));
    char *action_script = 
        zphoto_asprintf("getURL('%s', '_blank');", url);
    SWFAction action = compileSWFActionCode(action_script);
    free(action_script);
    free(url);
    return action;
}

static void
add_ending_frames (ZphotoFlashMaker *maker, SWFMovieClip clip, Photo *photo)
{
    SWFMovieClip_add(clip, compileSWFActionCode("gotoAndPlay('loaded');"));
    SWFMovieClip_nextFrame(clip);
}

static void
add_interval_frames (ZphotoFlashMaker *maker, 
		     SWFMovieClip clip,
		     Photo *photo,
		     float zooming_scale,
		     Point zooming_offset)
{
    if (! maker->disable_captions) {
        add_caption(maker, clip, photo->caption, photo->size, 
                    zooming_scale, zooming_offset);
    }

    SWFMovieClip_add(clip, compileSWFActionCode("Mouse.hide();"));
    SWFMovieClip_add(clip, compileSWFActionCode("var i = 0;"));
    SWFMovieClip_nextFrame(clip);

    {
        char *action_script;
        SWFMovieClip_labelFrame(clip, "blink");
        action_script = zphoto_asprintf(
            "   alpha = 77 + 23 * Math.abs(Math.cos(i/30));"
            "   i++;");
        SWFMovieClip_add(clip, compileSWFActionCode(action_script));
        free(action_script);
        SWFMovieClip_nextFrame(clip);
        SWFMovieClip_add(clip, compileSWFActionCode("gotoAndPlay('blink');"));
        SWFMovieClip_nextFrame(clip);
    }

    SWFMovieClip_labelFrame(clip, "open");
    SWFMovieClip_add(clip, create_open_action(photo));
    SWFMovieClip_nextFrame(clip);

    {
        char *action_script = zphoto_asprintf(
            "    _alpha = 100;"
            "    caption._visible = false;"
            "    gotoAndPlay(_currentframe + %d - nframes);" ,
            maker->zooming_nframes);
        SWFMovieClip_labelFrame(clip, "zoom_out");
        SWFMovieClip_add(clip, compileSWFActionCode(action_script));
        free(action_script);
        SWFMovieClip_nextFrame(clip);
    }
}

static void
create_zooming_frame_normal (ZphotoFlashMaker *maker, 
			     SWFDisplayItem item,
			     Photo *photo, 
			     int i)
{
    Point zooming_offset = calc_zooming_offset(maker, photo->size, i);
    float zooming_scale = calc_zooming_scale(maker, i);
    SWFDisplayItem_scaleTo(item, zooming_scale, zooming_scale);
    SWFDisplayItem_moveTo(item, zooming_offset.x, zooming_offset.y);
}


static void
create_zooming_in_frame_art (ZphotoFlashMaker *maker, 
                             SWFDisplayItem item,
                             Photo *photo,
                             int i)
{
    float x = (float)i / maker->zooming_nframes;
    create_zooming_frame_normal(maker, item, photo, i);
    SWFDisplayItem_setColorMult(item, 1.0, 1.0, 1.0, 1.0 - x);
}

static void
create_zooming_out_frame_art (ZphotoFlashMaker *maker, 
			      SWFDisplayItem item,
			      Photo *photo,
			      int i)
{
    float x = (float)i / maker->zooming_nframes;
    create_zooming_frame_normal(maker, item, photo, 0);
    SWFDisplayItem_setColorMult(item, 1.0, 1.0, 1.0, 1.0 - x);
}

static void
add_zooming_in_frames (ZphotoFlashMaker *maker, 
		       SWFMovieClip clip,
		       SWFDisplayItem item,
		       Photo *photo)
{
    int i;

/*    add_sound(clip, maker->mouse_over_sound); */
    SWFMovieClip_add(clip, compileSWFActionCode("swapDepths(0);"));
    SWFMovieClip_nextFrame(clip);
    for (i = 1; i <= maker->zooming_nframes; i++) {
	SWFMovieClip_add(clip, compileSWFActionCode("nframes++;"));
	maker->create_zooming_in_frame_func(maker, item, photo, i);
	SWFMovieClip_nextFrame(clip);
    }
}

static void
add_zooming_out_frames (ZphotoFlashMaker *maker, 
			SWFMovieClip clip,
			SWFDisplayItem item,
			Photo *photo)
{
    int i;
    for (i = maker->zooming_nframes - 1; i >= 1; i--) {
	SWFMovieClip_add(clip, compileSWFActionCode("nframes--;"));
	maker->create_zooming_out_frame_func(maker, item, photo, i);
	SWFMovieClip_nextFrame(clip);
    }
}

static SWFMovieClip
create_zooming_clip_normal (ZphotoFlashMaker *maker, 
			    Photo *photo, 
			    int x,
			    int y)
{
    SWFMovieClip clip;
    SWFDisplayItem item;

    clip  = newSWFMovieClip();
    item = SWFMovieClip_add(clip, create_shadowed_photo_inactive(maker, photo));

    add_starting_frames(maker, clip, item, photo);
    add_wait_frames(maker, clip, maker->wait_nframes);

    SWFMovieClip_remove(clip, item);
    item = SWFMovieClip_add(clip, create_shadowed_photo_active(maker, photo));

    add_zooming_in_frames(maker, clip, item, photo);
    add_interval_frames(maker, clip, photo, 
			calc_zooming_scale(maker, maker->zooming_nframes),
			calc_zooming_offset(maker, photo->size, 
					    maker->zooming_nframes));
    add_zooming_out_frames(maker, clip, item, photo);
    add_ending_frames(maker, clip, photo);
    return clip;
}

static SWFMovieClip
create_zooming_clip_art (ZphotoFlashMaker *maker, 
			 Photo *photo, 
			 int x,
			 int y)
{
    SWFMovieClip clip;
    SWFDisplayItem item;

    clip  = newSWFMovieClip();
    item = SWFMovieClip_add(clip, create_shadowed_photo_inactive(maker, photo));

    add_starting_frames(maker, clip, item, photo);

    SWFMovieClip_remove(clip, item);
    item = SWFMovieClip_add(clip, create_shadowed_photo_active(maker, photo));

    add_zooming_in_frames(maker, clip, item, photo);
    add_wait_frames(maker, clip, maker->invisible_nframes);
    SWFMovieClip_remove(clip, item);
    item = SWFMovieClip_add(clip, create_shadowed_photo_inactive(maker, photo));

    add_zooming_out_frames(maker, clip, item, photo);
    add_ending_frames(maker, clip, photo);
    return clip;
}

#define max(x, y)	((x) > (y) ? (x)  : (y))
#define min(x, y)	((x) < (y) ? (x)  : (y))
static float
calc_photo_scale (ZphotoFlashMaker *maker, Size size)
{
    float x, y;

    x = (float)maker->photo_x_unit / size.width  * (1.0 - maker->photo_margin);
    y = (float)maker->photo_y_unit / size.height * (1.0 - maker->photo_margin);

    return min(x, y);
}

static SWFAction
create_zoom_in_action (Photo *photo)
{
    char *action_script = zphoto_asprintf(
        "    setTarget('/photo_' + _root.current_id);"
        "    gotoAndPlay('zoom_out');"
        "    _root.current_id = %d;"
        "    _root.first_time = 0;"
        "    setTarget('/photo_' + _root.current_id);"
        "    gotoAndPlay('zoom_in');", 
        photo->id);
    SWFAction action =  compileSWFActionCode(action_script);
    free(action_script);
    return action;
}

static SWFAction
create_zoom_out_action (Photo *photo)
{
    char *action_script = 
	zphoto_asprintf("Mouse.show(); photo_%d.gotoAndPlay('zoom_out');",
			photo->id);
    SWFAction action =  compileSWFActionCode(action_script);
    free(action_script);
    return action;
}

static SWFShape
create_hit_region (Size size)
{
    SWFShape shape = newSWFShape();
    SWFFill  fill  = SWFShape_addSolidFill(shape, 0, 0, 0, 0);; 
    SWFShape_setRightFill(shape, fill);
    draw_rectangle(shape, size);
    return shape;
}

static SWFButton
create_button (ZphotoFlashMaker *maker, Photo *photo, int x, int y)
{
    SWFButton button;

    button = newSWFButton();

    SWFButton_addShape(button, 
		       create_hit_region(photo->size),
		       SWFBUTTON_HIT);
    SWFButton_addAction(button, 
			create_open_action(photo), 
			SWFBUTTON_MOUSEUP);
    SWFButton_addAction(button, 
			create_zoom_in_action(photo),
			SWFBUTTON_MOUSEOVER);
    SWFButton_addAction(button, 
			create_zoom_out_action(photo),
			SWFBUTTON_MOUSEOUT);
     return button;
}

typedef void	(*ArrangeEachFunc)	(ZphotoFlashMaker *maker,
					 SWFMovie clip, 
					 Photo *photo,
					 int x,
					 int y,
					 Point point);
static void
arrange_each (ZphotoFlashMaker *maker, 
	      SWFMovie movie, 
	      ArrangeEachFunc arrange_each_func,
	      ZphotoProgress *progress)
{
    int i;
    Point origin = calc_origin(maker);
    zphoto_progress_start(progress,"flash", N_("Creating a flash..."),
                          maker->nphotos);
    for (i = 0; i < maker->nphotos; i++) {
	int x = i % maker->x_nphotos;
	int y = i / maker->x_nphotos;
	Point point;
        point.x = origin.x + x * maker->photo_x_unit;
        point.y = origin.y + y * maker->photo_y_unit;

	zphoto_progress_set(progress, i,
			    zphoto_basename(maker->photos[i]->full_size_file_name));
	arrange_each_func(maker, movie, maker->photos[i], x, y, point);
    }
    zphoto_progress_finish(progress);
}

static void
arrange_anim (ZphotoFlashMaker *maker, 
	      SWFMovie movie, 
	      Photo *photo,  
	      int x, 
	      int y, 
	      Point point)
{
    float scale;
    SWFMovieClip clip;
    SWFButton button;
    SWFDisplayItem item;
    char *tmp_id = zphoto_asprintf("photo_%d", photo->id);

    load_photo(maker, photo);
    scale = calc_photo_scale(maker, photo->size);

    clip = maker->create_zooming_clip_func(maker, photo, x, y);
    item  = SWFMovie_add(movie, clip);

    SWFDisplayItem_moveTo(item, point.x, point.y);
    SWFDisplayItem_scale(item, scale, scale);
    SWFDisplayItem_setName(item, tmp_id);
    free(tmp_id);

    button  = create_button(maker, photo, x, y);
    item  = SWFMovie_add(movie, button);
    SWFDisplayItem_moveTo(item, point.x, point.y);
    SWFDisplayItem_scale(item, scale, scale);
    SWFMovie_nextFrame(movie);
}

static float
center (float base_width, float width)
{
    return (base_width - width) / 2;
}

static SWFMovieClip
create_progress_bar_housing (ZphotoFlashMaker *maker)
{
    SWFMovieClip clip;
    SWFShape housing;
    Size housing_size;
    Text text;

    housing_size.width  = maker->progress_bar_size.width;
    housing_size.height = maker->progress_bar_size.height;

    clip    = newSWFMovieClip();
    housing = newSWFShape();
    SWFShape_setLineStyle(housing, 
			  maker->progress_bar_housing_line_width,
			  maker->progress_bar_housing_color.r,
			  maker->progress_bar_housing_color.g,
			  maker->progress_bar_housing_color.b,
			  maker->progress_bar_housing_color.a);;
    draw_rectangle(housing, housing_size);
    text = create_text(maker, "now loading...", 
		       maker->progress_bar_text_height, 
		       maker->progress_bar_text_color);
    SWFMovieClip_add(clip, text.text);

    SWFMovieClip_add(clip, housing);
    SWFMovieClip_nextFrame(clip);
    return clip;
}

static SWFMovieClip
create_progress_bar (ZphotoFlashMaker *maker)
{
    SWFMovieClip clip = newSWFMovieClip();
    SWFShape shape = newSWFShape();
    SWFFill  fill  = SWFShape_addSolidFill(shape, 
					   maker->progress_bar_color.r,
					   maker->progress_bar_color.g,
					   maker->progress_bar_color.b,
					   maker->progress_bar_color.a);
    Size size;
    size.width  = maker->progress_bar_size.width;
    size.height = maker->progress_bar_size.height;

    SWFShape_setRightFill(shape, fill);
    draw_rectangle(shape, size);

    SWFMovieClip_add(clip, shape);
    SWFMovieClip_nextFrame(clip);
    return clip;
}

static Point
calc_progress_bar_point (ZphotoFlashMaker *maker)
{
    Point point;
    point.x = center(maker->flash_size.width,  maker->progress_bar_size.width);
    point.y = center(maker->flash_size.height * maker->album_margin / 2,
                     maker->progress_bar_size.height);
    return point;
}

static void
add_preload_anim (ZphotoFlashMaker *maker, SWFMovie movie)
{
    SWFMovieClip clip;
    SWFDisplayItem item;
    SWFAction action;
    Point progress_bar_point;

    clip = newSWFMovieClip();
    progress_bar_point = calc_progress_bar_point(maker);
    item = SWFMovieClip_add(clip, create_progress_bar_housing(maker));
    SWFDisplayItem_setName(item, "progress_bar_housing");
    SWFDisplayItem_moveTo(item, progress_bar_point.x, progress_bar_point.y);

    item = SWFMovieClip_add(clip, create_progress_bar(maker));
    SWFDisplayItem_setName(item, "progress_bar");
    SWFDisplayItem_moveTo(item, progress_bar_point.x, progress_bar_point.y);
    SWFDisplayItem_addAction(item, compileSWFActionCode(
                                 "_width = 0; var nframes = 0;"),
			     SWFACTION_ONLOAD);
    SWFMovieClip_nextFrame(clip);

    action = compileSWFActionCode(
        "if (_root.getBytesLoaded() >= _root.getBytesTotal()) {"
        "	progress_bar._width = progress_bar_housing._width;"
        "	stop();"
        "	_visible = false;"
	"} else {"
        "	var scale = _root.getBytesLoaded() / _root.getBytesTotal();"
	"	progress_bar._width = progress_bar_housing._width * scale;"
        "        var alpha = 50 + 50 * Math.abs(Math.cos(nframes/10));"
        "        progress_bar_housing._alpha = alpha;"
        "        nframes++;"
	"	gotoAndPlay(1); "
	"}");

    SWFMovieClip_add(clip, action);
    SWFMovieClip_nextFrame(clip);
    SWFMovie_add(movie, clip);
    SWFMovie_nextFrame(movie);
}

static void
terminate_movie (ZphotoFlashMaker *maker, SWFMovie movie)
{
    SWFMovie_add(movie, compileSWFActionCode("stop();"));
    SWFMovie_nextFrame(movie);
}

static void
save_movie (ZphotoFlashMaker *maker, SWFMovie movie, 
	    const char *file_name)
{
    SWFMovie_setRate(movie, maker->flash_rate);
    SWFMovie_setDimension(movie, 
			  maker->flash_size.width, maker->flash_size.height);
    SWFMovie_setBackground(movie, 
			   maker->background_color.r,
			   maker->background_color.g,
			   maker->background_color.b);
#if defined(MING_0_2a)
    SWFMovie_save(movie, file_name);
#else
    SWFMovie_save(movie, file_name, 1);
#endif

}

static Size
calc_canvas_size (ZphotoFlashMaker *maker, float ratio)
{
    float bottom_margin;
    Size canvas_size;

    canvas_size.width = maker->flash_size.width *
	(1.0 - maker->album_margin);
    bottom_margin = maker->flash_size.height * 0.05;
    canvas_size.height = maker->flash_size.height *
	(1.0 - maker->album_margin * ratio) - maker->bottom_margin;
    return canvas_size;
}

static void
define_cursor_key (ZphotoFlashMaker *maker, 
                   SWFButton button,
                   int key,
                   int value)
{
    char *action_script = zphoto_asprintf(
        "    setTarget('/photo_' + _root.current_id);"
        "    gotoAndPlay('zoom_out');"
        "    var tmp = _root.current_id + %d;"
        "    if (_root.first_time == 1) {"
        "        _root.current_id = 0;"
        "        _root.first_time = 0;"
        "    } else if (tmp >= 0 && tmp < %d) {"
	"        _root.current_id = tmp;"
	"    }"
        "    setTarget('/photo_' + _root.current_id);"
        "    gotoAndPlay('zoom_in');",
        value,
        maker->nphotos
        );
    SWFButton_addAction(button,
                        compileSWFActionCode(action_script),
                        SWFBUTTON_KEYPRESS(key));
    free(action_script);
}

static void
define_open_key (ZphotoFlashMaker *maker, SWFButton button, int key)
{
    char *action_script = zphoto_asprintf(
        "    setTarget('/photo_' + _root.current_id);"
        "    gotoAndPlay('open');"
        );
    SWFButton_addAction(button,
                        compileSWFActionCode(action_script),
                        SWFBUTTON_KEYPRESS(key));
    free(action_script);
}

static void
define_keys (ZphotoFlashMaker *maker, SWFMovie movie)
{
    SWFButton button  = newSWFButton();
    SWFMovie_add(movie, compileSWFActionCode(
                     "var current_id = 0;"
                     "var first_time = 1;"));
    SWFMovie_nextFrame(movie);
    define_cursor_key(maker, button, ' ', 1);

    /*
     * Emacs
     */
    define_cursor_key(maker, button, 'f', 1);
    define_cursor_key(maker, button, 'b', -1);
    define_cursor_key(maker, button, 'n', maker->x_nphotos);
    define_cursor_key(maker, button, 'p', -maker->x_nphotos);

    /*
     * Arrow keys
     */
    define_cursor_key(maker, button, 1, -1); /* left */
    define_cursor_key(maker, button, 2, 1); /* right */
    define_cursor_key(maker, button, 15, maker->x_nphotos); /* down */
    define_cursor_key(maker, button, 14, -maker->x_nphotos); /* up */

    /*
     * vi
     */
    define_cursor_key(maker, button, 'l', 1);
    define_cursor_key(maker, button, 'h', -1);
    define_cursor_key(maker, button, 'j', maker->x_nphotos);
    define_cursor_key(maker, button, 'k', -maker->x_nphotos);

    define_open_key(maker, button, 'o');
    define_open_key(maker, button, 13); /* enter */
    SWFMovie_add(movie, button);
    SWFMovie_nextFrame(movie);
}

void
zphoto_flash_maker_make (ZphotoFlashMaker *maker, 
			 const char *file_name,
			 ZphotoProgress *progress)
{
    SWFMovie movie = newSWFMovie();
    define_keys(maker, movie);

    add_preload_anim(maker, movie);
    arrange_each(maker, movie, arrange_anim, progress);
    terminate_movie(maker, movie);
    save_movie(maker, movie, file_name);

    destroySWFMovie(movie);
}

void
zphoto_flash_maker_set_art_mode (ZphotoFlashMaker *maker)
{
    maker->create_zooming_clip_func = create_zooming_clip_art;
    maker->create_zooming_in_frame_func = create_zooming_in_frame_art;
    maker->create_zooming_out_frame_func = create_zooming_out_frame_art;
}

void
zphoto_flash_maker_set_fade_effect (ZphotoFlashMaker *maker)
{
    maker->add_transition_func = add_transition_with_fade_effect;
}

void
zphoto_flash_maker_set_caption_options (
    ZphotoFlashMaker *maker, 
    int disable_captions)
{
    maker->disable_captions = disable_captions;
}

void
zphoto_flash_maker_set_background_color (ZphotoFlashMaker *maker, 
                                         const char *background_color)
{
    maker->background_color = translate_color(background_color);
}

void
zphoto_flash_maker_set_photo_colors (ZphotoFlashMaker *maker,
                                     const char *border_active_color,
                                     const char *border_inactive_color,
                                     const char *shadow_color)
{
    maker->border_active_color   = translate_color(border_active_color);
    maker->border_inactive_color = translate_color(border_inactive_color);
    maker->shadow_color          = translate_color(shadow_color);
}

void
zphoto_flash_maker_set_caption_colors (ZphotoFlashMaker *maker,
                                       const char *caption_border_color,
                                       const char *caption_frame_color,
                                       const char *caption_text_color)
{
    maker->caption_border_color = translate_color(caption_border_color);
    maker->caption_frame_color = translate_color(caption_frame_color);
    maker->caption_text_color = translate_color(caption_text_color);
}

void
zphoto_flash_maker_set_progress_bar_colors (ZphotoFlashMaker *maker,
                                            const char *progress_bar_color,
                                            const char 
                                            *progress_bar_text_color,
                                            const char 
                                            *progress_bar_housing_color)
{
    maker->progress_bar_color = translate_color(progress_bar_color);
    maker->progress_bar_text_color = 
        translate_color(progress_bar_text_color);
    maker->progress_bar_housing_color = 
        translate_color(progress_bar_housing_color);
}

void
zphoto_flash_maker_destroy (ZphotoFlashMaker *maker)
{
    int i;

    for (i = 0; i < maker->nphotos; i++) {
	destroy_photo(maker->photos[i]);
    }
    free(maker->photos);
    free(maker);
}

ZphotoFlashMaker*
zphoto_flash_maker_new (char **full_size_file_names,
                        char **thumbnail_file_names, 
                        char **html_file_names,
                        char **captions,
                        time_t *time_stamps, 
                        int nphotos,
                        int nsamples,
                        int flash_width,
                        int flash_height,
                        const char *flash_font_name)
{
    ZphotoFlashMaker *maker;
    float zooming_width, ratio;

    maker = zphoto_emalloc(sizeof(ZphotoFlashMaker));

    maker->flash_size.width       = flash_width;
    maker->flash_size.height      = flash_height;
    maker->flash_font_name   = zphoto_strdup(flash_font_name);

    /*
     * Default values.
     */
    maker->flash_rate   = 30;
    maker->album_margin   = 0.25;
    maker->zooming_nframes = 10;
    maker->wait_nframes = 5;
    maker->invisible_nframes = 7;
    maker->photo_margin = 0.12;

    maker->caption_height = 100;
    maker->caption_margin = 0.3;
    maker->caption_offset = maker->caption_height * maker->caption_margin / 2;
    maker->caption_width_ratio = 0.75;
    maker->progress_bar_text_height = maker->flash_size.height / 28;

    maker->progress_bar_size.width  = maker->flash_size.width  * 0.69;
    maker->progress_bar_size.height = maker->flash_size.height * 0.03;
    maker->progress_bar_housing_line_width = 20;


    maker->border_inactive_line_width = 1;
    maker->border_active_line_width = 150;

    maker->caption_border_line_width = 1;

    maker->photos = collect_photos(full_size_file_names,
                                   thumbnail_file_names, 
                                   html_file_names,
                                   captions,
                                   time_stamps,
                                   nphotos);
    maker->nphotos = nphotos;

    maker->nsamples = nsamples;
    maker->transition_nframes = 20;

    maker->disable_captions = 0;

    /*
     * Default colors.
     */
    maker->background_color = translate_color(ZPHOTO_BACKGROUND_COLOR);
    maker->border_inactive_color =
        translate_color(ZPHOTO_BORDER_INACTIVE_COLOR);
    maker->border_active_color = translate_color(ZPHOTO_BORDER_ACTIVE_COLOR);
    maker->shadow_color = translate_color(ZPHOTO_SHADOW_COLOR);
    maker->caption_border_color = translate_color(ZPHOTO_CAPTION_BORDER_COLOR);
    maker->caption_frame_color = translate_color(ZPHOTO_CAPTION_FRAME_COLOR);
    maker->caption_text_color = translate_color(ZPHOTO_CAPTION_TEXT_COLOR);
    maker->progress_bar_color = translate_color(ZPHOTO_PROGRESS_BAR_COLOR);
    maker->progress_bar_text_color = 
        translate_color(ZPHOTO_PROGRESS_BAR_TEXT_COLOR);
    maker->progress_bar_housing_color =
        translate_color(ZPHOTO_PROGRESS_BAR_HOUSING_COLOR);

    /* 
     * 2/3 for digital camera 
     */
    ratio = 2.0 / 3.0 * maker->flash_size.width / maker->flash_size.height;
    maker->x_nphotos = sqrt(ratio * nphotos);
    maker->y_nphotos = sqrt((1 / ratio)  * nphotos);
    if (maker->x_nphotos * maker->y_nphotos < nphotos)
	maker->y_nphotos++;
    if (maker->x_nphotos * maker->y_nphotos < nphotos)
	maker->x_nphotos++;

    maker->create_zooming_clip_func = create_zooming_clip_normal;
    maker->create_zooming_in_frame_func  = create_zooming_frame_normal;
    maker->create_zooming_out_frame_func = create_zooming_frame_normal;

    maker->add_transition_func = add_transition;

    /* 
     * 0.02 for the space for captions at the bottom. (ad hoc adjustment)
     */
    maker->bottom_margin = maker->flash_size.height * 0.02;

    maker->canvas_size = calc_canvas_size(maker, ratio);

    maker->photo_x_unit = maker->canvas_size.width  / maker->x_nphotos;
    maker->photo_y_unit = maker->canvas_size.height / maker->y_nphotos;

    zooming_width  = maker->flash_size.width - maker->canvas_size.width
	+ maker->photo_x_unit;
    maker->zooming_scale  = (float)zooming_width / maker->photo_x_unit;

    return maker;
}

