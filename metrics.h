/*
 * metrics.h
 *
 *  Created on: 22 Jun 2013
 *      Author: ntuckett
 */

#ifndef METRICS_H_
#define METRICS_H_

extern void cumulative_time_metric_start(const char *metric_name);
extern void cumulative_time_metric_stop(const char *metric_name);

extern void metrics_report();

#endif /* METRICS_H_ */
