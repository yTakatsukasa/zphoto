#ifndef __ZPHOTO_H__
#define __ZPHOTO_H__

#include <stdio.h> /* for FILE* */
#include <time.h>
#include <dirent.h>
#include <popt.h>
#include <stdarg.h> /* for va_list */
#include <setjmp.h> /* for jmp_buf */

/*
 * For xgettext.
 */
#ifndef N_
    #define N_(Text) Text
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Zphoto                 Zphoto;
typedef struct _ZphotoConfig           ZphotoConfig;
typedef struct _ZphotoFlashMaker       ZphotoFlashMaker;
typedef struct _ZphotoImageCopier      ZphotoImageCopier;
typedef struct _ZphotoTemplate         ZphotoTemplate;
typedef struct _ZphotoProgress         ZphotoProgress;
typedef struct _ZphotoAlist {
    char *key;
    char *value;
    struct _ZphotoAlist *next;
} ZphotoAlist;

typedef void    (*ZphotoProgressFunc)   (ZphotoProgress *progress);
typedef void    (*ZphotoXprintfFunc)    (const char *fmt, va_list args);

struct _ZphotoProgress {
    char                *task;
    char                *task_long;
    int                 current;
    int                 previous;
    int                 total;
    time_t              start_time;
    clock_t             start_processer_time;
    int                 is_finished;
    ZphotoProgressFunc  func;
    void                *data;
    const char          *file_name;
    jmp_buf             jmpbuf;
    int                 abort_p;
};

typedef struct _MetaConfig {
    char *key;
    void *storage;
    char *type;
    char *long_option;
    int  short_option;
    char *help;
    char *help_arg;
    struct _MetaConfig *next;
} MetaConfig;

struct _ZphotoConfig {
    MetaConfig  *meta;
    poptContext popcon;
    char        *zphoto_url;
    char        *zip_command;
    int         html_thumbnail_width;
    char        *preview_prefix;
    char        *thumbnail_prefix;
    int         nargs;
    char        **args;

    /*
     * Parameters that can be set by command line options
     */
    int         photo_width;
    int         thumbnail_width;
    float       gamma;
    char        *output_dir;
    char        *template_dir;
    char        *html_suffix;
    char        *flash_filename;
    int         flash_width;
    int         flash_height;
    char        *flash_font;
    char        *title;
    char        *caption_file;
    char        *zip_filename;
    int         art;
    int         disable_captions;
    int         include_original;
    int         caption_by_filename;
    int         sequential;
    int         sort_by_filename;
    int         no_sort;
    int         no_zip;
    int         no_exif;
    int         movie_nsamples;
    int         no_fade;
    int         quiet;

    char        *background_color;
    char        *border_inactive_color;
    char        *border_active_color;
    char        *shadow_color;
    char        *caption_border_color;
    char        *caption_frame_color;
    char        *caption_text_color;
    char        *progress_bar_color;
    char        *progress_bar_text_color;
    char        *progress_bar_housing_color;

    char        *css_background_color;
    char        *css_text_color;
    char        *css_footer_color;
    char        *css_horizontal_line_color;
    char        *css_photo_border_color;
    char        *css_thumbnail_border_color;
    char        *css_navi_link_color;
    char        *css_navi_visited_color;
    char        *css_navi_border_color;
    char        *css_navi_hover_color;
};


/*
 * zphoto.c
 */
Zphoto*         zphoto_new               (ZphotoConfig *config);
void            zphoto_add_file_names    (Zphoto *zphoto, 
                                          char **file_names,
                                          int nfile_names);
void            zphoto_destroy           (Zphoto *zphoto);
int             zphoto_get_nsteps        (Zphoto *zphoto);
void            zphoto_make_all          (Zphoto *zphoto);
void		zphoto_set_progress      (Zphoto *zphoto, 
                                          ZphotoProgressFunc func,
                                          void *data);
void            zphoto_abort             (Zphoto *zphoto);
char*           zphoto_get_output_dir    (Zphoto *zphoto);

/*
 * config.c
 */
ZphotoConfig*   zphoto_config_new                (void);
ZphotoConfig*   zphoto_config_read_rcfile        (ZphotoConfig *config);
void            zphoto_config_destroy            (ZphotoConfig *config);
void            zphoto_config_parse              (ZphotoConfig *config, 
                                                  int argc, char **argv);
void            zphoto_config_write               (ZphotoConfig *config, FILE *fp);
void            zphoto_config_save_rcfile        (ZphotoConfig *config);
void            zphoto_config_reset_flash_colors (ZphotoConfig *config);
void            zphoto_config_reset_css_colors   (ZphotoConfig *config);


/*
 * flash.c
 */
ZphotoFlashMaker*
zphoto_flash_maker_new (char **full_size_file_names,
                        char **thumbnail_file_names, 
                        char **html_file_names,
                        char **descriptions,
                        time_t *time_stamps, 
                        int nphotos,
                        int nsamples,
                        int flash_width,
                        int flash_height,
                        const char *flash_font_name);
void            zphoto_flash_maker_destroy      (ZphotoFlashMaker 
                                                 *maker);
void            zphoto_flash_maker_set_frontal_zooming (ZphotoFlashMaker
                                                 *maker);
void            zphoto_flash_maker_set_diagonal_zooming (ZphotoFlashMaker
                                                 *maker);
void            zphoto_flash_maker_set_art_mode (ZphotoFlashMaker
                                                 *maker);
void            zphoto_flash_maker_set_caption_options(ZphotoFlashMaker
                                                       *maker, 
                                                       int disable_captions);
void            zphoto_flash_maker_set_background_color(ZphotoFlashMaker 
                                                         *maker,
                                                         const char 
                                                         *background_color);
void            zphoto_flash_maker_set_photo_colors (ZphotoFlashMaker *maker,
                                                     const char 
                                                     *border_active_color,
                                                     const char
                                                     *border_inactive_color,
                                                     const char
                                                     *shadow_color);
void            zphoto_flash_maker_set_caption_colors (ZphotoFlashMaker *maker,
                                                       const char
                                                       *caption_border_color,
                                                       const char
                                                       *caption_frame_color,
                                                       const char
                                                       *caption_text_color);
void            zphoto_flash_maker_set_progress_bar_colors (ZphotoFlashMaker
                                                            *maker,
                                                            const char
                                                            *progress_bar_color,
                                                            const char
                                                            *progress_bar_text_color,
                                                            const char
                                                            *progress_bar_housing_color);
void            zphoto_flash_maker_set_fade_effect (ZphotoFlashMaker
                                                    *maker);

void            zphoto_flash_maker_make         (ZphotoFlashMaker *maker, 
                                                 const char *file_name,
                                                 ZphotoProgress *progress);

/*
 * image.cpp
 */
ZphotoImageCopier*      zphoto_image_copier_new         (void);
void                    zphoto_image_copier_set_width   (ZphotoImageCopier 
                                                         *copier, 
                                                         int width);
void                    zphoto_image_copier_set_gamma   (ZphotoImageCopier 
                                                         *copier, 
                                                         double gamma);

void                    zphoto_image_copier_set_prefix  (ZphotoImageCopier 
                                                         *copier, 
                                                         const char *prefix);
void                    zphoto_image_copier_destroy     (ZphotoImageCopier
                                                         *copier);
void                    zphoto_image_copier_copy        (ZphotoImageCopier
                                                         *copier,
                                                         const char *src,
                                                         const char *dest,
                                                         time_t time);
void                    zphoto_image_get_size           (const char *file_name,
                                                         int *width, 
                                                         int *height);
char**                  zphoto_image_save_samples       (const char *file_name,
                                                         const char *sample_file_name,
                                                         int nsamples);
time_t                  zphoto_image_get_time           (const char *file_name,
                                                         int no_exif);
unsigned char *         zphoto_image_get_bitmap         (const char* file_name, 
                                                         int *width, int *height);

/*
 * template.c
 */
ZphotoTemplate*         zphoto_template_new             (const char 
                                                         *template_file_name);
void                    zphoto_template_destroy         (ZphotoTemplate
                                                         *templ);
void                    zphoto_template_write           (ZphotoTemplate
                                                         *templ,
                                                         const char 
                                                         *output_file_name);
void                    zphoto_template_add_subst       (ZphotoTemplate 
                                                         *templ, 
                                                         const char *key, 
                                                         const char *value);

/*
 * progress.c
 */
ZphotoProgress* zphoto_progress_new             (void);

void            zphoto_progress_start           (ZphotoProgress *progress,
                                                 const char *task, 
                                                 const char *task_long, 
                                                 int total);
void            zphoto_progress_finish          (ZphotoProgress *progress);
                                                
void            zphoto_progress_destroy         (ZphotoProgress *progress);

void            zphoto_progress_set             (ZphotoProgress *progress, 
                                                 int count,
                                                 const char *file_name);
void            zphoto_progress_set_func        (ZphotoProgress *progress,
                                                 ZphotoProgressFunc func);
void            zphoto_progress_set_data        (ZphotoProgress *progress,
                                                 void *data);
void            zphoto_progress_abort           (ZphotoProgress *progress);

/*
 * alist.c
 */
ZphotoAlist*    zphoto_alist_add        (ZphotoAlist *alist, 
                                         const char *key, 
                                         const char *value);
char*           zphoto_alist_get        (ZphotoAlist *alist,
                                         const char *key);
void            zphoto_alist_destroy    (ZphotoAlist *alist);

/*
 * util.c
 */
void    zphoto_eprintf                  (const char *fmt, ...);
void    zphoto_wprintf                  (const char *fmt, ...);
void	zphoto_set_xprintf              (ZphotoXprintfFunc func);

char*   zphoto_asprintf                 (const char *fmt, ...);
FILE*   zphoto_efopen                   (const char *file_name, 
                                         const char *mode);
void*   zphoto_emalloc                  (size_t n);
void    zphoto_mkdir                    (const char *dir_name);
time_t  zphoto_get_mtime                (const char *file_name);
char*   zphoto_strdup                   (const char *str);
int     zphoto_file_p                   (const char *file_name);
DIR*    zphoto_eopendir                 (const char *dir_name);
char*   zphoto_time_string              (time_t mtime);
char*   zphoto_get_suffix               (const char *file_name);
char*   zphoto_suppress_suffix          (char *file_name);
char*   zphoto_modify_suffix            (const char *file_name, 
                                         const char *suffix);
char*   zphoto_basename                 (const char *file_name);
char*   zphoto_dirname                  (const char *file_name);
int     zphoto_exif_file_p              (const char *file_name);
time_t  zphoto_exif_get_time            (const char *file_name);
int     zphoto_supported_file_p         (const char *file_name);
int     zphoto_image_file_p             (const char *file_name);
int     zphoto_movie_file_p             (const char *file_name);
int     zphoto_web_file_p               (const char *file_name);
int     zphoto_dot_file_p               (const char *file_name);
int     zphoto_path_exist_p             (const char *file_name);
int     zphoto_support_movie_p          (void);
int     zphoto_support_image_p          (void);
int     zphoto_support_zip_p            (void);
int     zphoto_support_browser_p        (void);
int     zphoto_platform_w32_p           (void);
int     zphoto_platform_w32_jpn_p       (void);
void    zphoto_chdir_if_w32             (const char *command_path);
void    zphoto_init_magick              (void);
void    zphoto_finalize_magick          (void);
char*   zphoto_d_name_workaround        (struct dirent *d);
int     zphoto_blank_line_p             (const char *line);
int     zphoto_complete_line_p          (const char *line);
void    zphoto_chomp                    (char *line);
int     zphoto_strsuffixcasecmp         (const char *str1, const char *str2);
int     zphoto_valid_color_string_p     (const char *string);
void	zphoto_decode_color_string      (const char *string, 
                                         int *r, int *g, int *b, int *a);
char*	zphoto_encode_color_string      (int r, int g, int b, int a);
void    zphoto_launch_browser           (const char *url);
int     zphoto_path_separator           (void);
char*   zphoto_expand_path              (const char *path, const char *dir_name);
int     zphoto_directory_p              (const char *dir_name);
int     zphoto_directory_empty_p        (const char *dir_name);
char*	zphoto_escape_url		(const char *url);
char*	zphoto_get_program_file_name    (void);
char**  zphoto_get_image_suffixes       (void);
char**  zphoto_get_movie_suffixes       (void);


#define ZPHOTO_BACKGROUND_COLOR           "#ffffff"
#define ZPHOTO_BORDER_INACTIVE_COLOR      "#00008866"
#define ZPHOTO_BORDER_ACTIVE_COLOR        "#ffffffcc"
#define ZPHOTO_SHADOW_COLOR               "#0000ff44"
#define ZPHOTO_CAPTION_BORDER_COLOR       "#ffffffcc"
#define ZPHOTO_CAPTION_FRAME_COLOR        "#ffffffcc"
#define ZPHOTO_CAPTION_TEXT_COLOR         "#000077ff"
#define ZPHOTO_PROGRESS_BAR_COLOR         "#00ff0033"
#define ZPHOTO_PROGRESS_BAR_TEXT_COLOR    "#ff7f0077"
#define ZPHOTO_PROGRESS_BAR_HOUSING_COLOR "#000088cc"

#define ZPHOTO_CSS_BACKGROUND_COLOR        "#ffffff"
#define ZPHOTO_CSS_TEXT_COLOR              "#444488"
#define ZPHOTO_CSS_FOOTER_COLOR            "#888888"
#define ZPHOTO_CSS_HORIZONTAL_LINE_COLOR   "#8888bb"
#define ZPHOTO_CSS_PHOTO_BORDER_COLOR      "#8888ff"
#define ZPHOTO_CSS_THUMBNAIL_BORDER_COLOR  "#4444cc"
#define ZPHOTO_CSS_NAVI_LINK_COLOR         "#0000ff"
#define ZPHOTO_CSS_NAVI_VISITED_COLOR      "#aa55cc"
#define ZPHOTO_CSS_NAVI_BORDER_COLOR       "#6666ff"
#define ZPHOTO_CSS_NAVI_HOVER_COLOR        "#ddddff"

#ifdef __cplusplus
}
#endif

#endif
