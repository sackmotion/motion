/*
 * mmaloutput.c
 *
 *  Created on: 13 Apr 2014
 *      Author: ntuckett
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
#include "mmaloutput.h"
#include "motion.h"

static void buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_output* output = (mmal_output*) port->userdata;
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: Output %s: buffer_callback - entry", output->name);
    mmal_queue_put(output->buffer_queue, buffer);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: Output %s: buffer_callback - exit", output->name);
}

int mmal_output_init(const char* name, mmal_output* output, struct MMAL_PORT_T* port, int flags)
{
    memset(output, 0, sizeof(mmal_output));

    strncpy(output->name, name, MMAL_OUTPUT_NAME_LEN);
    output->name[MMAL_OUTPUT_NAME_LEN] = 0;
    output->flags = flags;

    output->buffer_pool = mmal_pool_create(port->buffer_num, port->buffer_size);
    if (output->buffer_pool == NULL) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL Output %s: buffer pool creation failed", output->name);
        mmal_output_deinit(output);
        return -1;
    }

    output->buffer_queue = mmal_queue_create();
    if (output->buffer_queue == NULL ) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: MMAL Output %s: buffer queue creation failed", output->name);
        mmal_output_deinit(output);
        return -1;
    }

    output->port = port;
    return 0;
}

int mmal_output_enable(mmal_output* output)
{
    output->port->userdata = (struct MMAL_PORT_USERDATA_T*) output;
    MMAL_STATUS_T status = mmal_port_enable(output->port, buffer_callback);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Unable to enable output %s port : error %d", output->name, status);
        return -1;
    }

    return 0;
}

int mmal_output_send_buffers_to_port(mmal_output* output)
{
    int num = mmal_queue_length(output->buffer_pool->queue);

    for (int i = 0; i < num; i++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(output->buffer_pool->queue);

        if (!buffer) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Output %s: Unable to get a required buffer %d from pool queue", output->name, i);
            return -1;
        }

        if (mmal_port_send_buffer(output->port, buffer) != MMAL_SUCCESS) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Output %s: Unable to send a buffer to port (%d)", output->name, i);
            return -1;
        }
    }

    return 0;
}

size_t mmal_output_process_buffer(mmal_output* output, void* dest, size_t size)
{
    int buffer_complete = 0;
    size_t buffer_progress = 0;
    char* buffer_dest = (char*)dest;

    do {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_wait(output->buffer_queue);

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: Output %s - got buffer", output->name);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: cmd %d flags %08x size %d/%d at %08x",
                buffer->cmd, buffer->flags, buffer->length, buffer->alloc_size, buffer->data);
        if (output->flags & MMAL_OUTPUT_INCREMENTAL) {
            if (buffer->cmd == 0) {
                if (size >= buffer_progress + buffer->length) {
                    mmal_buffer_header_mem_lock(buffer);
                    memcpy(buffer_dest + buffer_progress, buffer->data, buffer->length);
                    mmal_buffer_header_mem_unlock(buffer);
                    buffer_progress += buffer->length;
                    buffer_complete = buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END;
                    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: buffer progress %d/%d", buffer_progress, size);
                }
                else {
                    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Output %s - buffer overflow", output->name);
                    buffer_complete = 1;
                    buffer_progress = 0;
                }
            }
        } else if (buffer->cmd == 0 && (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)) {
            if (buffer->length == size && dest != NULL) {
                mmal_buffer_header_mem_lock(buffer);
                memcpy(dest, buffer->data, size);
                mmal_buffer_header_mem_unlock(buffer);
                buffer_progress = buffer->length;
            }
            buffer_complete = 1;
        }

        mmal_buffer_header_release(buffer);

        if (output->port->is_enabled) {
            MMAL_STATUS_T status;
            MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(output->buffer_pool->queue);

            if (new_buffer) {
                status = mmal_port_send_buffer(output->port, new_buffer);
                MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: Output %s - new buffer returned", output->name);
            }

            if (!new_buffer || status != MMAL_SUCCESS)
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Unable to return a buffer to output %s port", output->name);
        }
    } while (!buffer_complete && output->port->is_enabled);

    return buffer_progress;
}

void mmal_output_deinit(mmal_output* output)
{
    if (output->buffer_queue != NULL) {
        mmal_queue_destroy(output->buffer_queue);
    }

    if (output->buffer_pool != NULL) {
        mmal_pool_destroy(output->buffer_pool);
    }

    if (output->port != NULL && output->port->is_enabled) {
        mmal_port_disable(output->port);
    }

    memset(output, 0, sizeof(mmal_output));
}


