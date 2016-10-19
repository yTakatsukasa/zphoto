/* 
 * zphoto - a zooming photo album generator.
 *
 * Copyright (C) 2000  Satoru Takabayashi <satoru@namazu.org>
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

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <setjmp.h>
#include "zphoto.h"
#include "config.h"

static void
progress_quiet (ZphotoProgress *progress)
{
    /* do nothing */
}

ZphotoProgress *
zphoto_progress_new (void)
{
    ZphotoProgress *progress;

    progress = zphoto_emalloc(sizeof(ZphotoProgress));
    progress->func = progress_quiet;
    progress->data = NULL;
    progress->abort_p = 0;

    return progress;
}

void
zphoto_progress_start (ZphotoProgress *progress,
                       const char *task, const char *task_long, int total)
{
    assert(total >=0 && task != NULL);

    progress->current   = 0;
    progress->previous  = 0;
    progress->total     = total;
    progress->task      = zphoto_strdup(task);
    progress->task_long = zphoto_strdup(task_long);
    progress->is_finished = 0;
    progress->start_processer_time =  clock();
    progress->start_time = time(NULL);
    progress->file_name = "";

    zphoto_progress_set(progress, 0, "");
}

void
zphoto_progress_finish (ZphotoProgress *progress)
{
    assert(progress->is_finished == 0);

    progress->current     = progress->total;
    progress->is_finished = 1;
    progress->func(progress);

    free(progress->task);
    free(progress->task_long);
}

void
zphoto_progress_destroy (ZphotoProgress *progress)
{
    free(progress);
}

void
zphoto_progress_set (ZphotoProgress *progress, 
		     int count, const char *file_name)
{
    assert(count >= progress->previous);

    progress->current = count;
    progress->file_name = file_name;
    progress->func(progress);
    progress->previous = count;

    /*
     * FIXME: No consideration for resource handling.
     * Some amount of memory will probably be leaked.
     */
    if (progress->abort_p) {
        longjmp(progress->jmpbuf, 1);
    }
}


void
zphoto_progress_set_func (ZphotoProgress *progress, ZphotoProgressFunc func)
{
    progress->func = func;
}

void
zphoto_progress_set_data (ZphotoProgress *progress, void *data)
{
    progress->data = data;
}

void
zphoto_progress_abort (ZphotoProgress *progress)
{
    progress->abort_p = 1;
}

