/*
 * mmalcam.h
 *
 *    Include file for mmalcam.c
 *
 *    Copyright 2013 by Nicholas Tuckett
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 */

#ifndef MMALCAM_H_
#define MMALCAM_H_

#include "interface/mmal/mmal.h"
#include "raspicam/RaspiCamControl.h"

typedef struct mmalcam_context *mmalcam_context_ptr;

typedef struct mmalcam_context {
    struct context *cnt;        /* pointer to parent motion
                                   context structure */
    int width;
    int height;
    int framerate;

    MMAL_COMPONENT_T *camera_component;
    MMAL_PORT_T *camera_capture_port;
    MMAL_POOL_T *camera_buffer_pool;
    MMAL_QUEUE_T *camera_buffer_queue;
    RASPICAM_CAMERA_PARAMETERS camera_parameters;

} mmalcam_context;

int mmalcam_start (struct context *);
int mmalcam_next (struct context *, unsigned char *);
void mmalcam_cleanup (struct mmalcam_context *);

#endif /* MMALCAM_H_ */
