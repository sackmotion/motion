/*
 * videosourceplugin.c
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */
#include "motion.h"

void video_source_plugins_init(struct context *cnt) {
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

