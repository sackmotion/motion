/*
 * metrics.c
 *
 *  Created on: 22 Jun 2013
 *      Author: ntuckett
 */
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "logger.h"

#define METRIC_NAME_MAX_LEN     256

static int cumulative_time = 0;
static int start_time_stamp = 0;
static char metric_name_buffer[METRIC_NAME_MAX_LEN] = "";

void cumulative_time_metric_start(const char *metric_name)
{
    if (!metric_name_buffer[0]) {
        strncpy(metric_name_buffer, metric_name, METRIC_NAME_MAX_LEN);
        metric_name_buffer[METRIC_NAME_MAX_LEN - 1] = 0;
    }

    start_time_stamp = get_elapsed_time_ms();
}

void cumulative_time_metric_stop(const char *metric_name)
{
    cumulative_time += get_elapsed_time_ms() - start_time_stamp;
}

void metrics_report()
{
    if (metric_name_buffer[0]) {
        MOTION_LOG(NTC, TYPE_CORE, NO_ERRNO, "%s: metric %s: %d ms\n", metric_name_buffer, cumulative_time);
    }
}
