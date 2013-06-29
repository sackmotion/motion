/*
 * filecam.h
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */

#ifndef FILECAM_H_
#define FILECAM_H_

typedef struct filecam_context *filecam_context_ptr;

typedef struct filecam_context {
    struct context *cnt;        /* pointer to parent motion
                                   context structure */
    FILE *capture_file;
} filecam_context;

extern void filecam_select_as_plugin(struct context *);

#endif /* FILECAM_H_ */
