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

#include <zphoto.h>
#include <stdlib.h>
#include "config.h"

static void
show_mini_help (void)
{
    printf("Usage: %s [OPTION...]...\n", PACKAGE);
    printf("Try `%s --help` for more information.\n", PACKAGE);
    exit(0);
}

int 
main (int argc, char **argv)
{
    ZphotoConfig *config;
    Zphoto *zphoto;

    zphoto_init_magick();


    config = zphoto_config_new();
    zphoto_config_read_rcfile(config);
    zphoto_config_parse(config, argc, argv);

    if (config->nargs == 0)
        show_mini_help();

    zphoto = zphoto_new(config);
    zphoto_add_file_names(zphoto, config->args, config->nargs);

    zphoto_make_all(zphoto);
    zphoto_destroy(zphoto);
    
    zphoto_config_destroy(config);

    zphoto_finalize_magick();
    return 0;
}
