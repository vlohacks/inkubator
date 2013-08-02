/* 
 * File:   player.c
 * Author: vlo
 * 
 * Player engine
 *
 * Created on 29. Juni 2013, 13:57
 */

#include <stdlib.h>
#include <stdio.h>
#include "player.h"
#include "protracker.h"
#include "effects_mod.h"
#include "math.h"

player_t * player_init(const uint16_t sample_rate) 
{
    player_t * player = (player_t *)malloc(sizeof(player_t));
    player->sample_rate = sample_rate;
    player->channels = 0;
   
    return player;
}

void player_set_module(player_t * player, module_t * module) 
{
    player->module = module;    

    player_init_channels(player);
    player_init_defaults(player);
    
    player->current_order = 0;
    player->next_order = 0;
    player->current_row = 0;
    player->next_row = 1;
    player->current_pattern = player->module->orders[player->current_order];
    player->tick_pos = 0;
    player->tick_duration = player_calc_tick_duration(player->bpm, player->sample_rate);
    player->do_break = 0;       // to initialize state 
    player->current_tick = 6;
    player->pattern_delay = 0;
    
    effects_mod_init(player->effect_map); 
}

void player_init_channels(player_t * player) 
{
    int i, j;
    
   
    for (i = 0; i < player->module->num_channels; i++) {
        player->channels[i].sample_num = 0;
        player->channels[i].sample_pos.u32 = 0;
        player->channels[i].volume = 64;
        for (j = 0; j < 16; j++)
                player->channels[i].effect_last_value[j] = 0;
        player->channels[i].current_effect_num = 0;
        player->channels[i].current_effect_value = 0;
        player->channels[i].vibrato_state = 0;
        player->channels[i].tremolo_state = 0;
        player->channels[i].pattern_loop_position = 0;
        player->channels[i].pattern_loop_count = 0;     
        player->channels[i].sample_delay = 0;
        
        // default pannings

	if (((i % 4) == 1) || ((i % 4) == 2))           // RLLR RLLR ...
	    player->channels[i].panning = 0x00;
	else
	    player->channels[i].panning = 0xff;
	break;
       
    }
    
}

/* render next samples to mix, update the player state
 */
uint8_t player_read(player_t * player, int8_t * output_mix)
{
    int k;
    
    // reaching new tick
    if (player->tick_pos <= 0) {
        
        // last tick reached, advance to next row
        if (player->current_tick == player->speed) {
            // if there is a pattern delay, don't advance to the next row
            if (player->pattern_delay) {
                player->pattern_delay--;
                player->pattern_delay_active = 1;
            } else {
                player->current_row = player->next_row;
                player->next_row++;
                player->pattern_delay_active = 0;
            }
            
            player->current_tick = 0;
            
            // advance to next order if last row played or break upcoming
            if ((player->current_row >= 64) || (player->do_break)) {
                
                player->current_order = player->next_order;
                player->next_order++;
                player->do_break = 0;
                
                player->pattern_delay_active = 0;
                
                // only if regular pattern end (no break)
                if ((player->current_row) >= 64) {
                    player->current_row = 0;
                    player->next_row = 1;
                }

                // loop if looping enabled 
                if (player->next_order >= player->module->num_orders) {
                    //if (player->loop_module) 
                    player->next_order = 0;
                }
                
                // end of song reached...
                if (player->current_order >= player->module->num_orders) 
                    return 0;
                

                // lookup pattern to play in order list
                player->current_pattern = player->module->orders[player->current_order];
            }

            // fetch new pattern data from module
            for (k = 0; k < player->module->num_channels; k++) {
                module_pattern_data_t * current_data = &(player->module->patterns[player->current_pattern].rows[player->current_row].data[k]);
                
                // special behaviour for sample / note delay
                if ((data->effect_num == 0xe) && ((data->effect_value >> 4) == 0xd)) {
                    player->channels[k].dest_period = data->period;
                    player->channels[k].dest_sample_num = data->sample_num;
                    return;
                }

                // set sample
                if (data->sample_num > 0) {
                    player->channels[channel_num].sample_num = data->sample_num;
                    player->channels[channel_num].volume = player->module.sample_headers[player->channels[channel_num].sample_num - 1].volume;
                }

                // set period (note)
                if (data->period_index != 0xff) {
                    // special hack for note portamento... TODO remove here
                    if (data->effect_num == 0x3) {
                        player->channels[channel_num].dest_period = pgm_read_byte(data->period_index);
                    } else {
                        if (!(player->pattern_delay_active)) {
                            player->channels[channel_num].period = data->period;
                            player->channels[channel_num].period_index = data->period_index;
                            player->channels[channel_num].sample_pos.u32 = 0;
                            //player_channel_set_frequency(player, player->channels[channel_num].period, channel_num);
                        }
                    }
                }

                player->channels[channel_num].vibrato_state = 0;
                //player->channels[channel_num].tremolo_state = 0;
                player->channels[channel_num].volume_master = 64;

                if ((data->effect_num) != 0x3 && (data->effect_num != 0x5)) {
                    if (data->period_index != 0xff)
                        player_channel_set_period(player, data->period_index, channel_num);
                        //player_channel_set_frequency(player, player->channels[channel_num].period, channel_num);
                }
                
                player->channels[k].current_effect_num = current_data->effect_num;
                player->channels[k].current_effect_value = current_data->effect_value;
            }

        }

        // maintain effects
        for (k=0; k < player->module->num_channels; k++) 
            (player->effect_map)[player->channels[k].current_effect_num](player, k);
        
        // go for next tick
        player->current_tick++;
        player->tick_pos = player->tick_duration;

    }

    // mixing
    

    nano_mix_t mix;
    
    mix.i16 = 0;
    for (k = 0; k < player->module->num_channels; k++) 
        mix.i16 += (player_channel_fetch_sample(player, k) * player->channels[k].volume);
    
    *output_mix = (mix.i8.u);
    
    player->tick_pos--;
    
    return 2;
}


void player_init_defaults(player_t * player) 
{
    player->bpm = 125;
    player->speed = 6;
}

uint16_t player_calc_tick_duration(const uint16_t bpm, const uint16_t sample_rate) 
{
    return (((sample_rate / (bpm / 60.0)) / 4.0) / 6.0);
}

void player_channel_set_period(player_t * player, const uint8_t period_index, const int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);
    module_sample_t * sample = &(player->module->samples[channel->sample_num - 1]);

    uint16_t period = protracker_periods_finetune[sample->header.finetune][period_index];
    player_channel_set_frequency(player, period, channel_num);
}


void player_channel_set_frequency(player_t * player, const uint16_t period, const int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);
    
    channel->frequency = protracker_paulafreq[player->paula_freq_index] / (period * 2.0f);

    printf("channel freq %i: %i \n", channel_num, channel->frequency);

    channel->sample_interval.u16.u = channel->frequency;
    channel->sample_interval.u32 /= (uint32_t)player->sample_rate;
}

int8_t player_channel_fetch_sample(player_t * player,  const int channel_num) 
{
    int8_t s;
    
    player_channel_t * channel = &(player->channels[channel_num]);
    int sample_index = channel->sample_num - 1;

    // maintain looping
    if (player->module->samples[sample_index].header.loop_length > 2) {
        while (channel->sample_pos.u16.u >= (player->module->samples[sample_index].header.loop_length + player->module->samples[sample_index].header.loop_start)) {
            channel->sample_pos.u16.u -= player->module->samples[sample_index].header.loop_length;
        }
    } else {
        if (channel->sample_pos.u16.u >= player->module->samples[sample_index].header.length)
            return 0.0f;
    }

    // no sample, no sound... 
    if (channel->sample_num == 0)
        return 0;

    // trying to play a empty sample slot... play silence instead of segfault :)
    if (player->module->samples[sample_index].data == 0)
        return 0;
    
    s = player->module->samples[sample_index].data[channel->sample_pos.u16.u];

    // advance sample position
    channel->sample_pos.u32 += channel->sample_interval.u32;

    return s;
}
