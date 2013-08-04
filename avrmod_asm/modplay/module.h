/* 
 * File:   module.h
 * Author: vlo
 * 
 * Internal module structures
 *
 * Created on 29. Juni 2013, 13:22
 */

#ifndef MODULE_H
#define	MODULE_H

#include <stdint.h>

typedef struct {
    uint32_t loop_start;
    uint32_t loop_length;
    uint32_t length;
    uint32_t sram_offset;    
    int8_t finetune;
    uint8_t volume;
} module_sample_header_t;

typedef struct {
    uint8_t sample_num;
    uint8_t period_index;
    uint8_t effect_num;
    uint8_t effect_value;
} module_pattern_data_t;

typedef struct {
    module_sample_header_t sample_headers[31];
    uint8_t num_channels;
    uint8_t num_samples;
    uint8_t num_patterns;
    uint8_t num_orders;
    uint8_t orders[128];
} module_t;


#endif	/* MODULE_H */

