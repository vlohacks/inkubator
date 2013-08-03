/* 
 * File:   player.h
 * Author: vlo
 * 
 * Player engine
 *
 * Created on 29. Juni 2013, 15:01
 */

#ifndef PLAYER_H
#define	PLAYER_H

#include "module.h"
#include "protracker.h"

typedef union {
    struct {
        uint16_t l;
        uint16_t u;
    } u16;
    uint32_t u32;
} ufix1616_t;

typedef union {
    struct {
        uint8_t l;        
        uint8_t u;        
    } i8;
    uint16_t i16;
} nano_mix_t;

typedef struct {
    uint8_t sample_num;
    uint32_t sample_pos;
    uint32_t sample_interval;
    //ufix1616_t sample_pos;
    //ufix1616_t sample_interval;
    uint8_t sample_delay;
    uint16_t period;
    uint8_t period_index;
    uint16_t dest_period;
    uint8_t dest_sample_num;
    uint8_t dest_volume;
    uint8_t panning;
    uint16_t frequency;
    uint8_t volume;
    uint8_t current_effect_num;
    uint8_t current_effect_value;
    uint8_t effect_last_value[16];
    int8_t vibrato_state;
    int8_t tremolo_state;
    uint8_t pattern_loop_position;
    uint8_t pattern_loop_count;
} player_channel_t;


struct player_t;

typedef void (*effect_callback_t)(struct player_t *, int);

struct player_t {

    player_channel_t channels[4];
    module_t * module;                                  // module to play
    
    effect_callback_t effect_map[16];                     // effects map (module format specific)

    uint8_t speed;                                      // current speed setting (ticks per row)
    uint8_t bpm;                                       // current bpm setting

    uint16_t tick_pos;                                     // current position in tick (internal)
    uint16_t tick_duration;                                // duration of one tick (gets calculated when set speed effect occurs)
    uint16_t sample_rate;                                  // samplerate, normally matches output device"s rate

    uint8_t current_tick;
    uint8_t current_pattern;
    uint8_t current_order;
    uint8_t current_row;

    uint8_t do_break;
    uint8_t next_order;
    uint8_t next_row;
    
    uint8_t pattern_delay;
    uint8_t pattern_delay_active;
};

typedef struct player_t player_t;

/* Protoypes
 */
void player_init(player_t * player, const uint16_t sample_rate);
void player_set_module(player_t * player, module_t * module);
uint8_t player_read(player_t * player, int8_t * output_mix);

void player_init_channels(player_t * player);
void player_init_defaults(player_t * player);
uint16_t player_calc_tick_duration(const uint16_t bpm, const uint16_t sample_rate);
void player_channel_set_period(player_t * player, const uint8_t period_index, const int channel_num);
void player_channel_set_frequency(player_t * player, const uint16_t period, const int channel_num);
//int8_t player_channel_fetch_sample(player_t * player,  const int channel_num) ;
uint8_t player_channel_fetch_sample(player_channel_t * channel,  module_sample_header_t * h) ;

#endif	/* PLAYER_H */

