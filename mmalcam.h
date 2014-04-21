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

#include "mmaloutput.h"

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
    struct RASPICAM_CAMERA_PARAMETERS *camera_parameters;
    struct MMAL_COMPONENT_T *preview_component;
    struct MMAL_CONNECTION_T *preview_connection;
    struct MMAL_COMPONENT_T *splitter_component;
    struct MMAL_CONNECTION_T *splitter_connection;
    struct MMAL_COMPONENT_T *resize_component;
    struct MMAL_CONNECTION_T *resize_connection;
    struct MMAL_COMPONENT_T *jpeg_component;
    struct MMAL_CONNECTION_T *jpeg_connection;

    struct mmal_output camera_output;
    struct mmal_output secondary_output;

} mmalcam_context;

extern void mmalcam_select_as_plugin(struct context *);

#endif /* MMALCAM_H_ */
