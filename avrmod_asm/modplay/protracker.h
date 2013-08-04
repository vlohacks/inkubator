/* 
 * File:   protracker.h
 * Author: vlo
 * 
 * Various protracker-related definitions and tables
 *
 * Created on 29. Juni 2013, 13:55
 */

#ifndef PROTRACKER_H
#define	PROTRACKER_H

#include <stdint.h>
#include <avr/pgmspace.h>

#define PROTRACKER_PERIOD_MAX 856
#define PROTRACKER_PERIOD_MIN 113

#define protracker_num_periods 36


/* Prototypes
 */
int8_t protracker_lookup_period_index(const uint16_t period);

#endif	/* PROTRACKER_H */

