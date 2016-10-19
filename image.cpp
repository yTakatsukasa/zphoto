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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <time.h>
#include <zphoto.h>
#include "config.h"

struct _ZphotoImageCopier {
    int		width;
    double	gamma;
    int		resize_p;
    int		effect_p;
};

static void	get_new_image_size (ZphotoImageCopier *copier, 
                                    int old_width,  int old_height, 
                                    int *new_width, int *new_height);

/*
 * Avifle depended codes.
 */
#if HAVE_AVIFILE && HAVE_IMLIB2

#include <avifile.h>
#include <avm_output.h>
#include <X11/Xlib.h>
#include <Imlib2.h>

typedef struct {
    avm::IReadFile* file;
    avm::IReadStream* stream;
    int width;
    int height;
    int nframes;
    int pos;
    bool seekable;
} Avifile;

static int
avifile_count_nframes (avm::IReadStream *stream)
{
    int nframes = 0;
    while (1) {
        int status = stream->ReadFrame(false);
        if (status == -1) 
            break;
        nframes++;
    }
    if (stream->Seek(0) != 0) 
        zphoto_eprintf("avifile_count_nframes: Seek failed");
    return nframes;
}

static Avifile *
avifile_new (const char *file_name)
{
    assert(file_name != NULL);
    avm::out.resetDebugLevels(-1);

    Avifile *avifile = (Avifile *)zphoto_emalloc(sizeof(Avifile));

    avifile->file = avm::CreateReadFile(file_name);
    if (avifile->file == NULL)
        return NULL;
    avifile->stream = avifile->file->GetStream(0, avm::IStream::Video);
    if (avifile->stream == NULL)
        return NULL;

    BITMAPINFOHEADER bih;
    avifile->stream->GetVideoFormat(&bih, sizeof(bih));
    avifile->stream->StartStreaming(NULL);

    avifile->width = bih.biWidth;
    avifile->height = bih.biHeight;

    avifile->pos = 0;
    if (avifile->stream->GetLength() == 0) {
	// such as MPEG.
        avifile->nframes = avifile_count_nframes(avifile->stream);
        avifile->seekable = false;
    } else { 
	// such as Motion JPEG.
        avifile->nframes = avifile->stream->GetLength();
        avifile->seekable = true;
    }

    return avifile;
}

int
avifile_seek (Avifile *avifile, int pos)
{
    assert(avifile->pos <= pos);

    /*
     * FIXME: I don't know why but ReadStreamV::Seek()
     * causes the subtle bug that causes occasional
     * segmentation fault and the resulting broken
     * images... So I dicided to always use this stupid loop
     * version of seek instead of ReadStreamV::Seek() even
     * if the avifile is a seekable format.
     */
    avifile->seekable = false;
    if (avifile->seekable) {
        avifile->stream->Seek(pos);
    } else {
        for (int i = 0; i < (pos - avifile->pos); i++) {
            int status = avifile->stream->ReadFrame(false);
            if (status == -1)
                zphoto_eprintf("avifile_get_frame: ReadFrame failed");
        }
    }
    avifile->pos = pos;
    return 0;
}

static Imlib_Image 
avifile_convert_to_imlib_image (Avifile *avifile, CImage *cimage)
{
    BitmapInfo info(cimage->Width(), cimage->Height(), 24);
    CImage *new_cimage = new CImage(cimage, &info);
    cimage->Release();

    Imlib_Image imlib_image = imlib_create_image(avifile->width, 
                                                 avifile->height);
    imlib_context_set_image(imlib_image);
    DATA32* canvas = imlib_image_get_data();

    int bpp = new_cimage->Bpp();
    int pixels = new_cimage->Pixels();
    uint8_t* data = new_cimage->Data();
    for (int i = 0; i < pixels; i++) {
        canvas[i] = (data[i * bpp +0] << 0) |
            (data[i * bpp + 1] << 8) |
            (data[i * bpp + 2] << 16);
    }
    new_cimage->Release();
    return imlib_image;
}

static Imlib_Image
avifile_get_frame (Avifile *avifile, int n)
{
    avifile_seek(avifile, n);

    CImage *image = avifile->stream->GetFrame(true);
    if (image == NULL) 
        return NULL;
    avifile->pos++;
    return avifile_convert_to_imlib_image(avifile, image);
}

static void
avifile_destroy (Avifile *avifile)
{
    avifile->stream->StopStreaming();
    delete avifile->file;
    free(avifile);
}

static Imlib_Image
avifile_load_image (const char* file_name)
{
    Avifile *avifile = avifile_new(file_name);
    Imlib_Image image = avifile_get_frame(avifile, 0);
    avifile_destroy(avifile);
    return image;
}

static Imlib_Image *
loader_load_samples (const char *file_name, int nsamples)
{
    int nframes = 0;
    Imlib_Image *images;

    Avifile *avifile = avifile_new(file_name);
    if (avifile->nframes <= nsamples) {
        images = (Imlib_Image *)
            zphoto_emalloc(sizeof(Imlib_Image) * (avifile->nframes + 1));
        for (int i = 0; i < avifile->nframes; i++) {
            images[i] = avifile_get_frame(avifile, i);
            nframes++;
        }
    } else {
        images = (Imlib_Image *)
            zphoto_emalloc(sizeof(Imlib_Image) * (nsamples + 1));
        images[0] = avifile_get_frame(avifile, 0);
        nframes++;
        for (int i = 1; i < nsamples; i++) {
            int x = (int)((avifile->nframes - 1) * 
                          ((float)i / (nsamples - 1)));
            images[i] = avifile_get_frame(avifile, x);
            nframes++;
        }
    }
    images[nframes] = NULL;
    avifile_destroy(avifile);
    return images;
}

extern "C" char **
zphoto_image_save_samples (const char *file_name, 
                           const char *sample_file_name,
                           int nsamples)
{
    int i;
    char **sample_file_names;
    char *sans_suffix = 
        zphoto_suppress_suffix(zphoto_strdup(sample_file_name));
    Imlib_Image *images = loader_load_samples(file_name, nsamples);

    if (images == NULL) 
        zphoto_eprintf("loader_load_samples failed: %s", 
                       file_name);

    for (i = 0; images[i] != NULL; i++);  /* count actual samples */
    nsamples = i;
    sample_file_names = (char **)zphoto_emalloc(sizeof(char*) * (nsamples +1));

    for (i = 0; i < nsamples; i++) {
        char *sample_file_name = zphoto_asprintf("%s.%d.jpg", sans_suffix, i);
        imlib_context_set_image(images[i]);
        imlib_save_image(sample_file_name);
        imlib_free_image();
        sample_file_names[i] = sample_file_name;
    }
    sample_file_names[i] = NULL;
    free(images);
    free(sans_suffix);
    return sample_file_names;
}

extern "C" unsigned char *
zphoto_image_get_bitmap (const char* file_name, int *width, int *height)
{
    Avifile *avifile = avifile_new(file_name);
    avifile_seek(avifile, 0);

    CImage *cimage = avifile->stream->GetFrame(true);
    avifile_destroy(avifile);
    if (cimage == NULL) 
        return NULL;

    BitmapInfo info(cimage->Width(), cimage->Height(), 24);
    CImage *new_cimage = new CImage(cimage, &info);
    int bpp       = new_cimage->Bpp();
    int pixels    = new_cimage->Pixels();
    uint8_t* data = new_cimage->Data();
    unsigned char *bitmap = (unsigned char *)zphoto_emalloc(pixels * bpp);
    memmove(bitmap, data, pixels * bpp);

    for (int i = 0; i < pixels * bpp; i += bpp) {
        // Swap red and blue values.
        uint8_t tmp = bitmap[i];
        bitmap[i] = bitmap[i + 2];
        bitmap[i + 2] = tmp;
    }
    *width  = cimage->Width();
    *height = cimage->Height();

    new_cimage->Release();
    cimage->Release();
   
    return bitmap;
}

#else

extern "C" char **
zphoto_image_save_samples (const char *file_name, 
                           const char *sample_file_name,
                           int nsamples)
{
    assert(0);
}

extern "C" unsigned char *
zphoto_image_get_bitmap (const char* file_name, int *width, int *height)
{
    assert(0);
}

#endif


/*
 * Imlib2 depended codes.
 */
#ifdef HAVE_IMLIB2

#include <X11/Xlib.h>
#include <Imlib2.h>

static Imlib_Image
load_image (const char* file_name)
{
    assert(file_name != NULL);
    if (zphoto_image_file_p(file_name)) {
        return imlib_load_image(file_name);
    } else if (zphoto_support_movie_p() &&
               zphoto_movie_file_p(file_name)) 
    {
#ifdef HAVE_AVIFILE
        return avifile_load_image(file_name);
#else
        assert(0);
#endif
    } else {
        assert(0);
    }
}

static void
apply_gamma_correction (double gamma)
{
    Imlib_Color_Modifier modifier;

    modifier = imlib_create_color_modifier();
    imlib_context_set_color_modifier(modifier);
    imlib_modify_color_modifier_gamma(gamma);
    imlib_apply_color_modifier();
    imlib_free_color_modifier();
}

static void
advanced_copy_image (ZphotoImageCopier *copier,
		     const char *input_file_name, 
		     const char *output_file_name) 
{

    Imlib_Image input_image, output_image;
    int old_width, old_height, new_width, new_height;

    input_image = load_image(input_file_name);
    if (input_image == NULL)
	zphoto_eprintf("load_image: %s is not a supported file",
		       input_file_name);

    imlib_context_set_image(input_image);
    imlib_context_set_blend(1);
    imlib_context_set_anti_alias(1);

    old_width  = imlib_image_get_width();
    old_height = imlib_image_get_height();

    get_new_image_size(copier, old_width, old_height, &new_width, &new_height);

    output_image = imlib_create_image(new_width, new_height);
    if (output_image == NULL)
	zphoto_eprintf("%s:", output_image);

    imlib_context_set_image(output_image);
    imlib_blend_image_onto_image(input_image, 0, 0, 0,
				 old_width, old_height, 0, 0, 
				 new_width, new_height);

    if (copier->gamma != 1.0)
	apply_gamma_correction(copier->gamma);

    imlib_context_set_image(output_image);
    imlib_save_image(output_file_name);

    imlib_context_set_image(input_image);
    imlib_free_image();
    imlib_context_set_image(output_image);
    imlib_free_image();
}

extern "C" void
zphoto_image_get_size (const char *file_name, int *width, int *height)
{
    Imlib_Image image = load_image(file_name);
    if (image == NULL)
	zphoto_eprintf("load_image: %s is not a supported file",
		       file_name);
    imlib_context_set_image(image);
    *width  = imlib_image_get_width();
    *height = imlib_image_get_height();
    imlib_free_image();
}

/*
 * ImageMagick depended codes.
 */
#elif HAVE_MAGICK
#include <magick/api.h>

static void
advanced_copy_image (ZphotoImageCopier *copier,
		     const char *input_file_name, 
		     const char *output_file_name) 
{
    int old_width, old_height, new_width, new_height;
    Image *image, *resized_image;
    ExceptionInfo exception;
    ImageInfo *image_info;

    GetExceptionInfo(&exception);
    image_info = CloneImageInfo(NULL);
    strcpy(image_info->filename, input_file_name);
    image = ReadImage(image_info, &exception);
    if (image == NULL)
        zphoto_eprintf("%s is not supported by ImageMagick", input_file_name);

    old_width  = image->columns;
    old_height = image->rows;
    get_new_image_size(copier, old_width, old_height, &new_width, &new_height);
        
    resized_image = ResizeImage(image, new_width, new_height, 
                                LanczosFilter, 1.0, &exception);
    if (resized_image == NULL)
        zphoto_eprintf("%s is not supported by ImageMagick", input_file_name);

    if (copier->gamma != 1.0) {
        char *gamma = zphoto_asprintf("%f", copier->gamma);
        GammaImage(resized_image, gamma);
        free(gamma);
    }

    strcpy(resized_image->filename, output_file_name);
    WriteImage(image_info, resized_image);
    DestroyImage(image);
    DestroyImage(resized_image);
    DestroyImageInfo(image_info);
    DestroyExceptionInfo(&exception);
}

extern "C" void
zphoto_image_get_size (const char *file_name, int *width, int *height)
{
    Image *image;
    ExceptionInfo exception;
    ImageInfo *image_info;

    GetExceptionInfo(&exception);
    image_info = CloneImageInfo(NULL);
    strcpy(image_info->filename, file_name);
    image = ReadImage(image_info, &exception);
    if (exception.severity != UndefinedException || image == NULL)
        zphoto_eprintf("%s is not supported by ImageMagick", file_name);

    *width  = image->columns;
    *height = image->rows;

    DestroyImage(image);
    DestroyImageInfo(image_info);
    DestroyExceptionInfo(&exception);
}

/*
 * No Imaging library is available.
 */
#else

static void
advanced_copy_image (ZphotoImageCopier *copier,
		     const char *input_file_name, 
		     const char *output_file_name) 
{
    assert(0); /* unsupported */
}

extern "C" void
zphoto_image_get_size (const char *file_name, int *width, int *height)
{
    assert(0); /* unsupported */
}

#endif


/*
 * Common functions.
 */

static void
get_new_image_size (ZphotoImageCopier *copier, 
                    int old_width,  int old_height, 
                    int *new_width, int *new_height)
{
    /*
     * Only the image larger than copier->width is resized.
     */
    if (copier->resize_p && old_width > copier->width)  {
        assert(copier->width != 0);
        *new_width = copier->width;
        *new_height = (int)((double)*new_width / old_width * old_height);
    } else {
        *new_width  = old_width;
        *new_height = old_height;
    }
}

static void
simple_copy_image (ZphotoImageCopier *copier,
		   const char *input_file_name, 
		   const char *output_file_name)
{
    int n;
    char buf[BUFSIZ];
    FILE *in  = zphoto_efopen(input_file_name,  "rb");
    FILE *out = zphoto_efopen(output_file_name, "wb");

    while ((n = fread(buf, 1, BUFSIZ, in)) > 0) {
	int nn = fwrite(buf, 1, n, out);
	if (n != nn)
	    zphoto_eprintf("%s:", output_file_name);
    }
    if (ferror(in))
	zphoto_eprintf("%s:", input_file_name);

    fclose(in);
    fclose(out);
}

static void
restore_mtime (const char *file_name, time_t mtime)
{
    struct utimbuf tb;

    tb.actime  = time(NULL);
    tb.modtime = mtime;
    if (utime(file_name, &tb))
	zphoto_eprintf("%s:", file_name);
}

static int
convert_needed_p (const char *src, const char *dest)
{
    char *suffix1 = strrchr(src,  '.');
    char *suffix2 = strrchr(dest, '.');

    return strcmp(suffix1, suffix2) != 0;
}

extern "C" void
zphoto_image_copier_copy (ZphotoImageCopier *copier,
			  const char *src,
			  const char *dest,
                          time_t time)
{
    int convert_p = convert_needed_p(src, dest);

    if (zphoto_movie_file_p(dest))
	simple_copy_image(copier, src, dest);
    else if (copier->effect_p || copier->resize_p || convert_p)
	advanced_copy_image(copier, src, dest);
    else
	simple_copy_image(copier, src, dest);

    restore_mtime(dest, time);
}

extern "C" void
zphoto_image_copier_set_width (ZphotoImageCopier *copier, int width)
{
    assert(width > 0);
    copier->resize_p = 1;
    copier->width  = width;
}

extern "C" void
zphoto_image_copier_set_gamma (ZphotoImageCopier *copier, double gamma)
{
    copier->effect_p = 1;
    copier->gamma = gamma;
}

extern "C" ZphotoImageCopier *
zphoto_image_copier_new (void)
{
    ZphotoImageCopier *copier;

    copier = (ZphotoImageCopier*)zphoto_emalloc(sizeof(ZphotoImageCopier));
    copier->effect_p  = 0;
    copier->resize_p  = 0;
    copier->width     = 0;
    copier->gamma     = 1.0;
    return copier;
}

extern "C" void
zphoto_image_copier_destroy (ZphotoImageCopier *copier)
{
    free(copier);
}

extern "C" time_t
zphoto_image_get_time (const char *file_name, int no_exif)
{
    time_t time = -1;
    if (no_exif) {
        time = zphoto_get_mtime(file_name);
    } else {
        time = zphoto_exif_get_time(file_name);
    }
    return time;
}

