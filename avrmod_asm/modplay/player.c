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
#include "../sram.h"
#include "../mmc/uart.h"

void player_init(player_t * player, const uint16_t sample_rate) 
{
    player->sample_rate = sample_rate;
   
}

void player_set_module(player_t * player, module_t * module) 
{
    player->module = module;    

    player_init_channels(player);
    player_init_defaults(player);
    
    player->current_order = 0;
    player->next_order = 1;
    player->current_row = 0;
    player->next_row = 0;
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
        player->channels[i].sample_index = 0;
        player->channels[i].sample_pos = 0;
        player->channels[i].volume = 0;
        player->channels[i].period_index = 0;
        player->channels[i].period = 0;
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
uint8_t player_read(player_t * player, uint8_t * output_mix)
{
    uint8_t k;
    
    
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
            uint32_t sram_addr = (player->current_pattern * 64 * 16) + (player->current_row * 16);
            
            for (k = 0; k < player->module->num_channels; k++) {
                
                uint8_t data[4];
                
                data[0] = sram_read_char(&sram_addr); // sample
                sram_addr++;
                data[1] = sram_read_char(&sram_addr); // period_index
                sram_addr++;
                data[2] = sram_read_char(&sram_addr); // effect
                sram_addr++;
                data[3] = sram_read_char(&sram_addr); // effect params
                sram_addr++;
                
                // special behaviour for sample / note delay
                if ((data[2] == 0xe) && ((data[3] >> 4) == 0xd) && (data[1] != 0xff)) {
                    player->channels[k].dest_period = pgm_read_word_near(protracker_periods_finetune + data[1]);
                    player->channels[k].dest_sample_index = data[0];
                    continue;
                }

                // set sample
                if (data[0] != 0xff) {
                    player->channels[k].sample_index = data[0];
                    player->channels[k].volume = player->module->sample_headers[player->channels[k].sample_index].volume;
                }

                // set period (note)
                if (data[1] != 0xff) {
                    
                    //PORTB |= (unsigned char)(1 << k);
                    // special hack for note portamento... TODO remove here
                    if (data[2] == 0x3) {
                        player->channels[k].dest_period = pgm_read_byte(protracker_periods_finetune + data[1]);
                    } else {
                        if (!(player->pattern_delay_active)) {
                            //player->channels[channel_num].period = data->period;
                            player->channels[k].period_index = data[1];
                            player->channels[k].sample_pos = 0;
                            player_channel_set_period(player, data[1], k);
                            player_channel_set_frequency(player, player->channels[k].period, k);
                            /*
                            uart_putc('p');
                            uart_putw_dec(player->channels[k].period);
                            uart_putc(' ');
                            uart_putc('f');
                            uart_putw_dec(player->channels[k].frequency);
                            uart_putc('\n');                            
                             */
                        }
                    }
                }

                player->channels[k].vibrato_state = 0;
                //player->channels[channel_num].tremolo_state = 0;
                //player->channels[channel_num].volume_master = 64;

                if ((data[2]) != 0x3 && (data[2] != 0x5)) {
                    if (data[1] != 0xff) {
                        player_channel_set_period(player, data[1], k);
                        player_channel_set_frequency(player, player->channels[k].period, k);
                    }
                }
                
                player->channels[k].current_effect_num = data[2];
                player->channels[k].current_effect_value = data[3];
                

            }

        }

        // maintain effects
        for (k=0; k < player->module->num_channels; k++) {
            (player->effect_map)[player->channels[k].current_effect_num](player, k);
        }
        
        // go for next tick
        //if(player->current_tick == 3)
        //    PORTB &= (unsigned char)(0xf0);
            
        player->current_tick++;
        player->tick_pos = player->tick_duration;

    }

    // mixing
    

    //nano_mix_t mix;
    uint16_t mix;
    
    //mix.i16 = 0;
    
    mix = player_mix(player->channels, player->module->sample_headers, 4);
    /*
    mix = 0;
    for (k = 0; k < player->module->num_channels; k++) 
        mix += (uint16_t)(player_channel_fetch_sample( &(player->channels[k]), &(player->module->sample_headers[player->channels[k].sample_index])) * (uint16_t)player->channels[k].volume);
    */
    
    *output_mix = (uint8_t)(mix >> 8);
    
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
    module_sample_header_t * sample_header = &(player->module->sample_headers[channel->sample_index]);

    
    player->channels[channel_num].period =  pgm_read_word_near(protracker_periods_finetune + (sample_header->finetune * protracker_num_periods) + period_index);
    //player_channel_set_frequency(player, period, channel_num);
}


void player_channel_set_frequency(player_t * player, const uint16_t period, const int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);
    
    //channel->frequency = 7093789.2 / (period * 2.0f);
    channel->frequency =  70937892ul / (period * 20);

    //channel->sample_interval.u16.u = channel->frequency;
    //channel->sample_interval.u32 /= (uint32_t)player->sample_rate;
    channel->sample_interval = ((uint32_t)channel->frequency << 8);
    channel->sample_interval /= (uint32_t)player->sample_rate;
}
/*
inline uint8_t player_channel_fetch_sample(player_channel_t * channel,  module_sample_header_t * h) 
{
    uint8_t s;
    uint32_t sram_addr;
    
    // no sample, no sound... 
    if (channel->sample_index == 0xff)
        return 0;

    // trying to play a empty sample slot... play silence instead of segfault :)
    if (h->length == 0)
        return 0;

    // maintain looping
    if (h->loop_length > 2) {
        if ((channel->sample_pos >> 8) >= h->loop_length) {
            channel->sample_pos = (h->loop_start << 8);
        }
    } else {
        if ((channel->sample_pos >> 8) >= h->length)
            return 0;
    }

    

    sram_addr = h->sram_offset + (uint32_t)(channel->sample_pos >> 8);
    s = sram_read_char(&sram_addr);
    //s = player->module->samples[sample_index].data[channel->sample_pos.u16.u];

    // advance sample position
    channel->sample_pos += channel->sample_interval;

    return s;
}
*/
/*
int8_t player_channel_fetch_sample(player_t * player,  const int channel_num) 
{
    int8_t s;
    uint32_t sram_addr;
    
    player_channel_t * channel = &(player->channels[channel_num]);
    int sample_index = channel->sample_num - 1;
    module_sample_header_t * h = &(player->module->sample_headers[sample_index]);
            
    // maintain looping
    if (h->loop_length > 2) {
        if (channel->sample_pos.u16.u >= (h->loop_length + h->loop_start)) {
            channel->sample_pos.u16.u = h->loop_start;
        }
    } else {
        if (channel->sample_pos.u16.u >= h->length)
            return 0;
    }

    // no sample, no sound... 
    if (channel->sample_num == 0)
        return 0;

    // trying to play a empty sample slot... play silence instead of segfault :)
    if (h->length == 0)
        return 0;
    
    sram_addr = h->sram_offset + (uint32_t)channel->sample_pos.u16.u;
    s = sram_read_char(&sram_addr);
    //s = player->module->samples[sample_index].data[channel->sample_pos.u16.u];

    // advance sample position
    channel->sample_pos.u32 += channel->sample_interval.u32;

    return s;
}
*/