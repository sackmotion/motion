/*
 * filecam.c
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */

#include "motion.h"
#include "rotate.h"

int filecam_start (struct context *cnt)
{
    filecam_context_ptr filecam;

    filecam = mymalloc(sizeof(filecam_context));
    memset(filecam, 0, sizeof(filecam_context));
    cnt->filecam = filecam;
    filecam->cnt = cnt;

    filecam->capture_file = fopen(cnt->conf.filecam_path, "rb");
    if (filecam->capture_file == NULL) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: filecam capture file %s could not be opened", cnt->conf.filecam_path);
        return -1;
    }

    cnt->imgs.width = cnt->conf.width;
    cnt->imgs.height = cnt->conf.height;
    cnt->imgs.size = (cnt->conf.width * cnt->conf.height * 3) / 2;
    cnt->imgs.motionsize = cnt->conf.width * cnt->conf.height;
    cnt->imgs.type = VIDEO_PALETTE_YUV420P;

    return 0;
}

int filecam_next (struct context *cnt, struct image_data* imgdat)
{
    if (!cnt || !cnt->filecam) {
        return NETCAM_FATAL_ERROR;
    }

    filecam_context_ptr filecam = cnt->filecam;

    if (filecam->capture_file) {
        if (fread(imgdat->image, 1, cnt->imgs.size, filecam->capture_file)) {
            if (cnt->rotate_data.degrees > 0)
                rotate_map(cnt, imgdat->image);
        }
        else {
            raise(SIGQUIT);
        }
    }

    return 0;
}

void filecam_cleanup (struct context *cnt)
{
    filecam_context_ptr filecam = cnt->filecam;

    if (filecam != NULL) {
        if (filecam->capture_file) {
            fclose(filecam->capture_file);
        }

        free(filecam);
    }
}

void filecam_select_as_plugin(struct context *cnt) {
    cnt->video_source.video_source_start_fn = filecam_start;
    cnt->video_source.video_source_next_fn = filecam_next;
    cnt->video_source.video_source_cleanup_fn = filecam_cleanup;
}

