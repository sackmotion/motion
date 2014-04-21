/*
 * sourceplugin.h
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */

#ifndef VIDEOSOURCEPLUGIN_H_
#define VIDEOSOURCEPLUGIN_H_

struct context;

typedef int (*video_source_start_ptr)(struct context *);
typedef int (*video_source_next_ptr)(struct context *, struct image_data* imgdat);
typedef void (*video_source_cleanup_ptr)(struct context *);

typedef struct video_source_plugin *video_source_plugin_ptr;

typedef struct video_source_plugin {
    video_source_start_ptr    video_source_start_fn;
    video_source_next_ptr     video_source_next_fn;
    video_source_cleanup_ptr  video_source_cleanup_fn;
} video_source_plugin;

extern void video_source_plugins_init(struct context *cnt);

#endif /* VIDEOSOURCEPLUGIN_H_ */
