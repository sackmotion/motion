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

typedef struct mmalcam_context *mmalcam_context_ptr;
struct MMAL_BUFFER_HEADER_T;

typedef struct mmalcam_context {
    struct context *cnt;        /* pointer to parent motion
                                   context structure */
    int width;
    int height;
    int framerate;
    int last_still_capture_time_ms;
    int still_capture_delay_ms;
    FILE *raw_capture_file;

    struct MMAL_COMPONENT_T *camera_component;
    struct MMAL_PORT_T *camera_capture_port;
    struct MMAL_POOL_T *camera_buffer_pool;
    struct MMAL_QUEUE_T *camera_buffer_queue;
    struct RASPICAM_CAMERA_PARAMETERS *camera_parameters;

    void (*camera_buffer_callback)(struct MMAL_PORT_T *port, struct MMAL_BUFFER_HEADER_T *buffer);
} mmalcam_context;

extern void mmalcam_select_as_plugin(struct context *);

#endif /* MMALCAM_H_ */
