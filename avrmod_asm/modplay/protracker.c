/* 
 * File:   protracker.c
 * Author: vlo
 * 
 * Various protracker-related helper functions
 *
 * Created on 29. Juni 2013, 13:57
 */

#include "protracker.h"

/* Lookup index for given period value
 */
int8_t protracker_lookup_period_index(const uint16_t period)
{
    int i;
    for (i = 0; i < protracker_num_periods; i++) {
        if (pgm_read_byte_near(protracker_periods_finetune + i) == period)
            return i;
    }
    return -1;
}




