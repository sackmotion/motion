/*
 * videosourceplugin.c
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */
#include "motion.h"

void video_source_plugins_init(struct context *cnt) {
    cnt->imgs.secondary_width = 0;
    cnt->imgs.secondary_height = 0;
    cnt->imgs.secondary_size = 0;

    struct config *conf = &cnt->conf;

    if (conf->filecam_path) {
        filecam_select_as_plugin(cnt);
        return;
    }
#ifdef HAVE_MMAL
    if (conf->mmalcam_name) {
        mmalcam_select_as_plugin(cnt);
        return;
    }
#endif
}

