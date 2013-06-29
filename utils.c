/*
 * utils.c
 *
 *  Created on: 16 Jun 2013
 *      Author: ntuckett
 */

#include <time.h>

struct timespec timespec_diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

int get_elapsed_time_ms()
{
    static int base_set = 0;
    static struct timespec base_tspec;
    struct timespec tspec;

    if (base_set == 0)
    {
        base_set = 1;
        clock_gettime(CLOCK_REALTIME, &base_tspec);
    }

    clock_gettime(CLOCK_REALTIME, &tspec);
    struct timespec diff = timespec_diff(base_tspec, tspec);

    return (diff.tv_nsec / 1000000) + (diff.tv_sec * 1000);
}


