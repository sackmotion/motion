/*
 * mmaloutput.h
 *
 *  Created on: 13 Apr 2014
 *      Author: ntuckett
 */

#ifndef MMALOUTPUT_H_
#define MMALOUTPUT_H_

#define MMAL_OUTPUT_NAME_LEN    63

#define MMAL_OUTPUT_INCREMENTAL 1   // Indicate the output gets filled by multiple buffer iterations

typedef struct mmal_output {
    char                    name[MMAL_OUTPUT_NAME_LEN + 1];
    struct MMAL_PORT_T*     port;
    struct MMAL_POOL_T*     buffer_pool;
    struct MMAL_QUEUE_T*    buffer_queue;
    int                     flags;
} mmal_output;

extern int mmal_output_init(const char* name, mmal_output* output, struct MMAL_PORT_T* output_port, int flags);
extern int mmal_output_enable(mmal_output* output);
extern int mmal_output_send_buffers_to_port(mmal_output* output);
extern size_t mmal_output_process_buffer(mmal_output* output, void* dest, size_t size);
extern void mmal_output_deinit(mmal_output* output);

#endif /* MMALOUTPUT_H_ */
