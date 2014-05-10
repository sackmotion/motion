/*
 * mmalcam.c
 *
 *    Raspberry Pi camera module using MMAL API.
 *
 *    Built upon functionality from the Raspberry Pi userland utility raspivid.
 *
 *    Copyright 2013 by Nicholas Tuckett
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#include "bcm_host.h"
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_port.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "raspicam/RaspiCamControl.h"

#include "motion.h"
#include "rotate.h"
#include "utils.h"

#define MMALCAM_OK		0
#define MMALCAM_ERROR	-1

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_STILLS_PORT 2
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define STILL_PREVIEW_WIDTH 320
#define STILL_PREVIEW_HEIGHT 240
#define STILL_FRAME_RATE_NUM 15
#define STILL_FRAME_RATE_DEN 1
#define STILL_FIRST_FRAME_DELAY_MS 2500
#define PREVIEW_FRAME_RATE_NUM 30
#define PREVIEW_FRAME_RATE_DEN 1

enum
{
    CAPTURE_MODE_VIDEO  = 1,
    CAPTURE_MODE_STILL  = 2,
};

static void parse_camera_control_params(const char *control_params_str, RASPICAM_CAMERA_PARAMETERS *camera_params)
{
    char *control_params_tok = alloca(strlen(control_params_str) + 1);
    strcpy(control_params_tok, control_params_str);

    char *next_param = strtok(control_params_tok, " ");

    while (next_param != NULL) {
        char *param_val = strtok(NULL, " ");
        if (raspicamcontrol_parse_cmdline(camera_params, next_param + 1, param_val) < 2) {
            next_param = param_val;
        } else {
            next_param = strtok(NULL, " ");
        }
    }
}

static void check_disable_port(MMAL_PORT_T *port)
{
    if (port && port->is_enabled) {
        mmal_port_disable(port);
    }
}

static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (buffer->cmd != MMAL_EVENT_PARAMETER_CHANGED) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Received unexpected camera control callback event, 0x%08x",
                buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}

static void set_port_format(int width, int height, MMAL_ES_FORMAT_T *format)
{
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = width;
    format->es->video.height = height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = width;
    format->es->video.crop.height = height;
}

static void set_video_port_format(mmalcam_context_ptr mmalcam, MMAL_ES_FORMAT_T *format)
{
    set_port_format(mmalcam->width, mmalcam->height, format);
    format->es->video.frame_rate.num = mmalcam->framerate;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
}

static MMAL_STATUS_T connect_ports(MMAL_PORT_T *source_port, MMAL_PORT_T *sink_port, MMAL_CONNECTION_T **connection)
{
   MMAL_STATUS_T status;

   status =  mmal_connection_create(connection, source_port, sink_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

   if (status == MMAL_SUCCESS) {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS) {
         MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Unable to enable connection: error %d", status);
         mmal_connection_destroy(*connection);
      }
   }
   else {
       MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Unable to create connection: error %d", status);
   }

   return status;
}

static int create_splitter_component(mmalcam_context_ptr mmalcam, MMAL_PORT_T *source_port)
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *splitter_component;
    MMAL_PORT_T *input_port;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &splitter_component);

    if (status != MMAL_SUCCESS) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Failed to create splitter component");
        goto error;
    }

    input_port = splitter_component->input[0];
    mmal_format_copy(input_port->format, source_port->format);
    input_port->buffer_num = 3;
    status = mmal_port_format_commit(input_port);
    if (status != MMAL_SUCCESS)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set splitter input port format : error %d", status);
        goto error;
    }

    for(int i = 0; i < splitter_component->output_num; i++)
    {
        MMAL_PORT_T *output_port = splitter_component->output[i];
        output_port->buffer_num = 3;
        mmal_format_copy(output_port->format,input_port->format);
        status = mmal_port_format_commit(output_port);
        if (status != MMAL_SUCCESS)
        {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set splitter output port format : error %d", status);
            goto error;
        }
    }

    mmalcam->splitter_component = splitter_component;
    return MMALCAM_OK;

    error:
    if (splitter_component) {
        mmal_component_destroy(splitter_component);
    }
    return MMALCAM_ERROR;
}

static int create_resize_component(mmalcam_context_ptr mmalcam, MMAL_PORT_T *source_port, int width, int height)
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *resize_component;
    MMAL_PORT_T *input_port;

    status = mmal_component_create("vc.ril.resize", &resize_component);

    if (status != MMAL_SUCCESS) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Failed to create resize component");
        goto error;
    }

    input_port = resize_component->input[0];
    mmal_format_copy(input_port->format, source_port->format);
    input_port->buffer_num = 3;
    status = mmal_port_format_commit(input_port);
    if (status != MMAL_SUCCESS)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set resize input port format : error %d", status);
        goto error;
    }

    MMAL_PORT_T *output_port = resize_component->output[0];
      output_port->buffer_num = 3;
    mmal_format_copy(output_port->format,input_port->format);
    output_port->format->es->video.width = width;
    output_port->format->es->video.height = height;
    output_port->format->es->video.crop.x = 0;
    output_port->format->es->video.crop.y = 0;
    output_port->format->es->video.crop.width = width;
    output_port->format->es->video.crop.height = height;

    status = mmal_port_format_commit(output_port);
    if (status != MMAL_SUCCESS)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set resize output port format : error %d", status);
        goto error;
    }

    mmalcam->resize_component = resize_component;
    return MMALCAM_OK;

    error:
    if (resize_component) {
        mmal_component_destroy(resize_component);
    }
    return MMALCAM_ERROR;
}

static int create_jpeg_component(mmalcam_context_ptr mmalcam)
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *jpeg_component;
    MMAL_PORT_T *input_port;
    MMAL_PORT_T *output_port;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &jpeg_component);

    if (status != MMAL_SUCCESS) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Failed to create jpeg component");
        goto error;
    }

    input_port = jpeg_component->input[0];
    output_port = jpeg_component->output[0];
    mmal_format_copy(output_port->format, input_port->format);

    output_port->format->encoding = MMAL_ENCODING_JPEG;
    output_port->buffer_size = output_port->buffer_size_recommended;
    output_port->buffer_num = output_port->buffer_num_recommended;

    if (output_port->buffer_size < output_port->buffer_size_min)
    {
        output_port->buffer_size = output_port->buffer_size_min;
    }
    if (output_port->buffer_num < output_port->buffer_num_min)
    {
        output_port->buffer_num = output_port->buffer_num_min;
    }

    status = mmal_port_format_commit(output_port);
    if (status != MMAL_SUCCESS)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set jpeg output port format : error %d", status);
        goto error;
    }

    status = mmal_port_parameter_set_uint32(output_port, MMAL_PARAMETER_JPEG_Q_FACTOR, mmalcam->cnt->conf.mmalcam_buffer2_jpeg);
    if (status != MMAL_SUCCESS)
    {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s:Couldn't set jpeg quality : error %d", status);
        goto error;
    }

    mmalcam->jpeg_component = jpeg_component;
    return MMALCAM_OK;

    error:
    if (jpeg_component) {
        mmal_component_destroy(jpeg_component);
    }
    return MMALCAM_ERROR;
}

static void destroy_components(mmalcam_context_ptr mmalcam);
static int create_camera_component(mmalcam_context_ptr mmalcam, const char *mmalcam_name, int capture_mode)
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *camera_component;
    MMAL_PORT_T *capture_port = NULL;

    status = mmal_component_create(mmalcam_name, &camera_component);

    if (status != MMAL_SUCCESS) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Failed to create MMAL camera component %s", mmalcam_name);
        goto error;
    }

    if (camera_component->output_num == 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "MMAL camera %s doesn't have output ports", mmalcam_name);
        goto error;
    }

    status = mmal_port_enable(camera_component->control, camera_control_callback);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = mmalcam->width,
            .max_stills_h = mmalcam->height,
            .stills_yuv422 = 0,
            .one_shot_stills = capture_mode == CAPTURE_MODE_STILL ? 1 : 0,
            .max_preview_video_w = mmalcam->width,      // these must match the chosen resolution otherwise
            .max_preview_video_h = mmalcam->height,     // video capture does not work
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC };

    mmal_port_parameter_set(camera_component->control, &cam_config.hdr);
    raspicamcontrol_set_all_parameters(camera_component, mmalcam->camera_parameters);

    MMAL_PORT_T *preview_port = camera_component->output[MMAL_CAMERA_PREVIEW_PORT];

    switch(capture_mode)
    {
        case CAPTURE_MODE_VIDEO:
        {
            set_video_port_format(mmalcam, preview_port->format);
            if (mmal_port_format_commit(preview_port)) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "camera setup couldn't configure preview");
                goto error;
            }

            capture_port = camera_component->output[MMAL_CAMERA_VIDEO_PORT];
            set_video_port_format(mmalcam, capture_port->format);
            capture_port->format->encoding = MMAL_ENCODING_I420;
            status = mmal_port_format_commit(capture_port);

            MMAL_PORT_T *stills_port = camera_component->output[MMAL_CAMERA_STILLS_PORT];
            mmal_format_full_copy(stills_port->format, preview_port->format);
            stills_port->format->es->video.frame_rate.num = 1;
            stills_port->format->es->video.frame_rate.den = 1;
            if (mmal_port_format_commit(stills_port)) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "video camera setup couldn't configure (unused) still port");
                goto error;
            }
            break;
        }

        case CAPTURE_MODE_STILL:
        {
            set_port_format(STILL_PREVIEW_WIDTH, STILL_PREVIEW_HEIGHT, preview_port->format);
            preview_port->format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
            preview_port->format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
            if (mmal_port_format_commit(preview_port)) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "camera setup couldn't configure preview");
                goto error;
            }

            capture_port = camera_component->output[MMAL_CAMERA_STILLS_PORT];
            set_port_format(mmalcam->width, mmalcam->height, capture_port->format);
            capture_port->format->encoding = MMAL_ENCODING_I420;
            capture_port->format->es->video.frame_rate.num = STILL_FRAME_RATE_NUM;
            capture_port->format->es->video.frame_rate.den = STILL_FRAME_RATE_DEN;

            // Duplicate preview format onto unused video port
            mmal_format_full_copy(camera_component->output[MMAL_CAMERA_VIDEO_PORT]->format, preview_port->format);
            if (mmal_port_format_commit(camera_component->output[MMAL_CAMERA_VIDEO_PORT])) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "still camera setup couldn't configure (unused) video port");
                goto error;
            }

            status = mmal_port_format_commit(capture_port);
            break;
        }
    }

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (capture_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        capture_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    status = mmal_component_enable(camera_component);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "camera component couldn't be enabled");
        goto error;
    }

    // Create a null sink for preview
    MMAL_COMPONENT_T *null_sink = NULL;
    if (capture_mode == CAPTURE_MODE_STILL) {
        status = mmal_component_create("vc.null_sink", &null_sink);
        if (status) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "null sink component couldn't be created");
            goto error;
        }

        status = mmal_component_enable(null_sink);
        if (status) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "null_sink component couldn't be enabled");
            goto error;
        }

        status = connect_ports(camera_component->output[MMAL_CAMERA_PREVIEW_PORT],
                               null_sink->input[0],
                               &mmalcam->preview_connection);
        if (status) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "preview connection setup failed");
            goto error;
        }
    }

    mmalcam->camera_component = camera_component;
    mmalcam->preview_component = null_sink;
    mmalcam->camera_capture_port = capture_port;
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "MMAL camera component created");
    return MMALCAM_OK;

    error:
    if (null_sink) {
        mmal_component_destroy(null_sink);
    }
    if (camera_component) {
        mmal_component_destroy(camera_component);
    }
    return MMALCAM_ERROR;
}

static void disable_components_and_ports(mmalcam_context_ptr mmalcam)
{
    if (mmalcam->jpeg_connection) {
        mmal_connection_destroy(mmalcam->jpeg_connection);
        mmalcam->jpeg_connection = NULL;
    }

    if (mmalcam->splitter_connection) {
        mmal_connection_destroy(mmalcam->splitter_connection);
        mmalcam->splitter_connection = NULL;
    }

    if (mmalcam->resize_connection) {
        mmal_connection_destroy(mmalcam->resize_connection);
        mmalcam->resize_connection = NULL;
    }

    if (mmalcam->preview_connection) {
        mmal_connection_destroy(mmalcam->preview_connection);
        mmalcam->preview_connection = NULL;
    }

    check_disable_port(mmalcam->camera_component->output[MMAL_CAMERA_VIDEO_PORT]);
    check_disable_port(mmalcam->camera_component->output[MMAL_CAMERA_STILLS_PORT]);

    if (mmalcam->jpeg_component) {
        mmal_component_disable(mmalcam->jpeg_component);
    }

    if (mmalcam->splitter_component) {
        mmal_component_disable(mmalcam->splitter_component);
    }

    if (mmalcam->resize_component) {
        mmal_component_disable(mmalcam->resize_component);
    }

    if (mmalcam->preview_component) {
        mmal_component_disable(mmalcam->preview_component);
    }

    if (mmalcam->camera_component) {
        mmal_component_disable(mmalcam->camera_component);
    }
}

static void destroy_components(mmalcam_context_ptr mmalcam)
{
    if (mmalcam->jpeg_component) {
        mmal_component_destroy(mmalcam->jpeg_component);
        mmalcam->jpeg_component = NULL;
    }
    if (mmalcam->splitter_component) {
        mmal_component_destroy(mmalcam->splitter_component);
        mmalcam->splitter_component = NULL;
    }
    if (mmalcam->resize_component) {
        mmal_component_destroy(mmalcam->resize_component);
        mmalcam->resize_component = NULL;
    }
    if (mmalcam->preview_component) {
        mmal_component_destroy(mmalcam->preview_component);
        mmalcam->preview_component = NULL;
    }
    if (mmalcam->camera_component) {
        mmal_component_destroy(mmalcam->camera_component);
        mmalcam->camera_component = NULL;
    }
}

/**
 * mmalcam_start
 *
 *      This routine is called from the main motion thread.  It's job is
 *      to open up the requested camera device via MMAL and do any required
 *      initialisation.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure for this device.
 *
 * Returns:     0 on success
 *              -1 on any failure
 */

int mmalcam_start(struct context *cnt)
{
    mmalcam_context_ptr mmalcam;

    cnt->mmalcam = (mmalcam_context*) mymalloc(sizeof(struct mmalcam_context));
    memset(cnt->mmalcam, 0, sizeof(mmalcam_context));
    mmalcam = cnt->mmalcam;
    mmalcam->cnt = cnt;

    MOTION_LOG(ALR, TYPE_VIDEO, NO_ERRNO,
            "%s: MMAL Camera thread starting... for camera (%s) of %d x %d at %d fps",
            cnt->conf.mmalcam_name, cnt->conf.width, cnt->conf.height, cnt->conf.frame_limit);

    mmalcam->camera_parameters = (RASPICAM_CAMERA_PARAMETERS*)malloc(sizeof(RASPICAM_CAMERA_PARAMETERS));
    if (mmalcam->camera_parameters == NULL) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "camera params couldn't be allocated");
        return MMALCAM_ERROR;
    }

    raspicamcontrol_set_defaults(mmalcam->camera_parameters);
    mmalcam->width = cnt->conf.width;
    mmalcam->height = cnt->conf.height;
    mmalcam->framerate = cnt->conf.frame_limit;

    if (mmalcam->width & 15) {
        mmalcam->width += 16 - (mmalcam->width & 15);
    }

    if (mmalcam->height & 15) {
        mmalcam->height += 16 - (mmalcam->height & 15);
    }

    if (cnt->conf.mmalcam_control_params) {
        parse_camera_control_params(cnt->conf.mmalcam_control_params, mmalcam->camera_parameters);
    }

    int capture_mode;

    if (cnt->conf.mmalcam_use_still) {
        MOTION_LOG(ALR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL Camera using still capture");
        capture_mode = CAPTURE_MODE_STILL;

        if (cnt->conf.minimum_frame_time > 0) {
            mmalcam->still_capture_delay_ms = cnt->conf.minimum_frame_time * 1000;
        }
        else {
            mmalcam->still_capture_delay_ms = 1000 / cnt->conf.frame_limit;
        }
    }
    else {
        MOTION_LOG(ALR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL Camera using video capture");
        capture_mode = CAPTURE_MODE_VIDEO;
    }

    cnt->imgs.width = mmalcam->width;
    cnt->imgs.height = mmalcam->height;
    cnt->imgs.size = (mmalcam->width * mmalcam->height * 3) / 2;
    cnt->imgs.motionsize = mmalcam->width * mmalcam->height;
    cnt->imgs.type = VIDEO_PALETTE_YUV420P;

    if (cnt->conf.mmalcam_buffer2_upscale > 0) {
        cnt->imgs.secondary_type = SECONDARY_TYPE_RAW;
        cnt->imgs.secondary_width = mmalcam->width * cnt->conf.mmalcam_buffer2_upscale;
        cnt->imgs.secondary_height = mmalcam->height * cnt->conf.mmalcam_buffer2_upscale;
        cnt->imgs.secondary_size = (cnt->imgs.secondary_width * cnt->imgs.secondary_height * 3) / 2;

        mmalcam->width *= cnt->conf.mmalcam_buffer2_upscale;
        mmalcam->height *= cnt->conf.mmalcam_buffer2_upscale;
    }

    int retval = create_camera_component(mmalcam, cnt->conf.mmalcam_name, capture_mode);

    if (cnt->imgs.secondary_size)
    {
        if (retval == 0) {
            retval = create_splitter_component(mmalcam, mmalcam->camera_capture_port);

            if (retval == 0) {
                retval = connect_ports(mmalcam->camera_capture_port, mmalcam->splitter_component->input[0], &mmalcam->splitter_connection);
            }
        }

        if (retval == 0) {
            retval = create_resize_component(mmalcam, mmalcam->splitter_component->output[1], cnt->imgs.width, cnt->imgs.height);

            if (retval == 0) {
                retval = connect_ports(mmalcam->splitter_component->output[1], mmalcam->resize_component->input[0], &mmalcam->resize_connection);
            }
        }

        if (retval == 0) {
            retval = mmal_output_init("mmalcam", &mmalcam->camera_output, mmalcam->resize_component->output[0], 0);
        }

        if (retval == 0) {
            if (cnt->conf.mmalcam_buffer2_jpeg > 0) {
                retval = create_jpeg_component(mmalcam);

                if (retval == 0) {
                    retval = connect_ports(mmalcam->splitter_component->output[0], mmalcam->jpeg_component->input[0], &mmalcam->jpeg_connection);

                    if (retval == 0) {
                        retval = mmal_output_init("secondary", &mmalcam->secondary_output, mmalcam->jpeg_component->output[0], MMAL_OUTPUT_INCREMENTAL);

                        if (retval == 0) {
                            cnt->imgs.secondary_type = SECONDARY_TYPE_JPEG;
                        }
                    }
                }
            }
            else {
                retval = mmal_output_init("secondary", &mmalcam->secondary_output, mmalcam->splitter_component->output[0], 0);
            }
        }

        if (retval == 0) {
            if (mmal_output_enable(&mmalcam->secondary_output)) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "MMAL resize output enabling failed");
                retval = MMALCAM_ERROR;
            }
        }
    }
    else {
        if (retval == 0) {
            retval = mmal_output_init("mmalcam", &mmalcam->camera_output, mmalcam->camera_capture_port, 0);
        }
    }

    if (retval == 0) {
        if (mmal_output_enable(&mmalcam->camera_output)) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "MMAL camera output enabling failed");
            retval = MMALCAM_ERROR;
        }
    }

    if (retval == 0) {
        if (mmal_port_parameter_set_boolean(mmalcam->camera_capture_port, MMAL_PARAMETER_CAPTURE, 1)
                != MMAL_SUCCESS) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "MMAL camera capture start failed");
            retval = MMALCAM_ERROR;
        }
        if (capture_mode == CAPTURE_MODE_STILL) {
            // Allow exposure to stabilise for first frame
            vcos_sleep(STILL_FIRST_FRAME_DELAY_MS);
        }
        mmalcam->last_still_capture_time_ms = get_elapsed_time_ms();
    }

    if (retval == 0) {
        retval = mmal_output_send_buffers_to_port(&mmalcam->camera_output);
    }

    if (retval == 0 && cnt->imgs.secondary_size) {
        retval = mmal_output_send_buffers_to_port(&mmalcam->secondary_output);
    }

    if (retval == 0) {
        if (mmalcam->cnt->conf.mmalcam_raw_capture_file) {
            mmalcam->raw_capture_file = fopen(mmalcam->cnt->conf.mmalcam_raw_capture_file, "wb");
            if (mmalcam->raw_capture_file == NULL) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL couldn't open raw capture file %s", mmalcam->cnt->conf.mmalcam_raw_capture_file);
            }
        }
    }
    return retval;
}

/**
 * mmalcam_cleanup
 *
 *      This routine shuts down any MMAL resources, then releases any allocated data
 *      within the mmalcam context and frees the context itself.
 *      This function is also called from motion_init if first time connection
 *      fails and we start retrying until we get a valid first frame from the
 *      camera.
 *
 * Parameters:
 *
 *      mmalcam          Pointer to a mmalcam context
 *
 * Returns:              Nothing.
 *
 */
void mmalcam_cleanup(struct context *cnt)
{
    mmalcam_context *mmalcam = cnt->mmalcam;

    MOTION_LOG(ALR, TYPE_VIDEO, NO_ERRNO, "MMAL Camera cleanup");

    if (mmalcam != NULL ) {

        disable_components_and_ports(mmalcam);
        mmal_output_deinit(&mmalcam->camera_output);
        mmal_output_deinit(&mmalcam->secondary_output);
        destroy_components(mmalcam);

        if (mmalcam->camera_parameters) {
            free(mmalcam->camera_parameters);
        }

        if (mmalcam->raw_capture_file) {
            fclose(mmalcam->raw_capture_file);
        }

        free(mmalcam);
    }
}

/**
 * mmalcam_next
 *
 *      This routine is called when the main 'motion' thread wants a new
 *      frame of video.  It fetches the most recent frame available from
 *      the Pi camera already in YUV420P, and returns it to motion.
 *
 * Parameters:
 *      cnt             Pointer to the context for this thread
 *      map             Pointer to a buffer for the returned image
 *      imgdat          Pointer
 *
 * Returns:             Error code
 */
int mmalcam_next(struct context *cnt, struct image_data* imgdat)
{
    mmalcam_context_ptr mmalcam;

    if ((!cnt) || (!cnt->mmalcam))
        return NETCAM_FATAL_ERROR;

    mmalcam = cnt->mmalcam;

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: mmalcam_next - start");
    mmal_output_process_buffer(&mmalcam->camera_output, imgdat->image, cnt->imgs.size);

    if (cnt->imgs.secondary_size && imgdat != NULL) {
        imgdat->secondary_size = mmal_output_process_buffer(&mmalcam->secondary_output, imgdat->secondary_image, cnt->imgs.secondary_size);
    }

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: mmalcam_next - buffer processing completed");

    if (mmalcam->cnt->conf.mmalcam_use_still) {
        int curr_time = get_elapsed_time_ms();
        int capture_time_delta = curr_time - mmalcam->last_still_capture_time_ms;
        if (capture_time_delta < mmalcam->still_capture_delay_ms)
        {
            vcos_sleep(mmalcam->still_capture_delay_ms - capture_time_delta);
        }

        // According to RaspiCam source, may need to set shutter speed each time
        mmal_port_parameter_set_uint32(mmalcam->camera_component->control, MMAL_PARAMETER_SHUTTER_SPEED,
                                        mmalcam->camera_parameters->shutter_speed);
        if (mmal_port_parameter_set_boolean(mmalcam->camera_capture_port, MMAL_PARAMETER_CAPTURE, 1)
                != MMAL_SUCCESS) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL camera capture start failed");
        }

        mmalcam->last_still_capture_time_ms = curr_time;
    }

    if (mmalcam->raw_capture_file) {
        fwrite(imgdat->image, 1, cnt->imgs.size, mmalcam->raw_capture_file);
    }

    if (cnt->rotate_data.degrees > 0)
        rotate_map(cnt, imgdat->image);

    return 0;
}

void mmalcam_select_as_plugin(struct context *cnt)
{
    cnt->video_source.video_source_start_fn = mmalcam_start;
    cnt->video_source.video_source_next_fn = mmalcam_next;
    cnt->video_source.video_source_cleanup_fn = mmalcam_cleanup;
}
