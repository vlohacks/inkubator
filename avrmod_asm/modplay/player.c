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

    player->tick_pos = 0;    
    player->tick_duration = player_calc_tick_duration(player->bpm, player->sample_rate);
    
    player->current_tick = 6;    
    player->current_order = 0;
    player->current_pattern = player->module->orders[player->current_order];
    player->current_row = 0;
    
    player->next_order = 1;
    player->next_row = 0;

    player->do_break = 0;       

    player->pattern_delay = 0;
    player->pattern_delay_active = 0;
    
    effects_mod_init(player->effect_map); 
}

void player_init_channels(player_t * player) 
{
    int i, j;
    
   
    for (i = 0; i < player->module->num_channels; i++) {
        player->channels[i].sample_index = 0xff;
        player->channels[i].sample_pos = 0;
        player->channels[i].sample_interval = 0;
        player->channels[i].volume = 64;
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

	if ((i == 1) || (i == 2))           // RLLR RLLR ...
	    player->channels[i].panning = 0x00;
	else
	    player->channels[i].panning = 0xff;
	break;
       
    }
    
}


void player_new_tick(player_t * player) 
{
    
    uint8_t k;
    
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
            //if (player->current_order >= player->module->num_orders) 
            //    return 0;


            // lookup pattern to play in order list
            player->current_pattern = player->module->orders[player->current_order];
        }

        // fetch new pattern data from module
        uint32_t sram_addr = ((uint32_t)player->current_pattern * 64 * 16) + ((uint32_t)player->current_row * 16);

        //uart_putdw_hex(sram_addr);
        //uart_putc('\n');
        player->ledstate = 0;
        player->ledstate = 16;
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

            /*
            for (int i=0; i<4; i++)
                uart_putc_hex(data[i]);
            uart_putc('\n');
            */

            // special behaviour for sample / note delay
            if ((data[2] == 0xe) && ((data[3] >> 4) == 0xd) && (data[1] != 0xff)) {
                player->channels[k].dest_sample_index = data[0];
                player->channels[k].dest_period =  player_period_by_index(data[1], player->module->sample_headers[data[0]].finetune);  //pgm_read_word_near(protracker_periods_finetune + data[1]);

                continue;
            }

            // set sample
            if (data[0] != 0xff) {
                player->channels[k].sample_index = data[0];
                player->channels[k].volume = player->module->sample_headers[data[0]].volume;
            }

            // set period (note)
            if (data[1] != 0xff) {

                player->ledstate |= (uint8_t)(1<<(uint8_t)k);


                //PORTB |= (unsigned char)(1 << k);
                // special hack for note portamento... TODO remove here
                if (data[2] == 0x3) {
                    player->channels[k].dest_period = player_period_by_index(data[1], player->module->sample_headers[player->channels[k].sample_index].finetune);  //pgm_read_byte(protracker_periods_finetune + data[1]);
                } else {
                    if (!(player->pattern_delay_active)) {
                        //player->channels[channel_num].period = data->period;
                        player->channels[k].period_index = data[1];
                        player->channels[k].sample_pos = 0;
                        //player_channel_set_period(player, data[1], k);
                        //player_channel_set_frequency(player, player->channels[k].period, k);
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
        /*
        if (player->current_tick == 0) {
            uart_putc_hex(player->current_pattern);
            uart_putc(' ');
            uart_putc_hex(player->current_row);
            uart_putc(' ');
            uart_putc_hex((uint8_t)k);
            uart_putc('\n');
        }
        */
        (player->effect_map)[player->channels[k].current_effect_num](player, k);
    }

    if (player->current_tick == 1)
        player->ledstate = 16;

    player->current_tick++;
    player->tick_pos = player->tick_duration;
    
}


/* render next samples to mix, update the player state
 */

#if 0
mix_t player_read(player_t * player)
{
    uint8_t k;
    
    //player->ledstate &= 0b11101111;
    
    // reaching new tick
    if (player->tick_pos == 0) {
        
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
                //if (player->current_order >= player->module->num_orders) 
                //    return 0;
                

                // lookup pattern to play in order list
                player->current_pattern = player->module->orders[player->current_order];
            }

            // fetch new pattern data from module
            uint32_t sram_addr = ((uint32_t)player->current_pattern * 64 * 16) + ((uint32_t)player->current_row * 16);
            
            //uart_putdw_hex(sram_addr);
            //uart_putc('\n');
            player->ledstate = 0;
            player->ledstate = 16;
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
                
                /*
                for (int i=0; i<4; i++)
                    uart_putc_hex(data[i]);
                uart_putc('\n');
                */
                
                // special behaviour for sample / note delay
                if ((data[2] == 0xe) && ((data[3] >> 4) == 0xd) && (data[1] != 0xff)) {
                    player->channels[k].dest_sample_index = data[0];
                    player->channels[k].dest_period =  player_period_by_index(data[1], player->module->sample_headers[data[0]].finetune);  //pgm_read_word_near(protracker_periods_finetune + data[1]);
                    
                    continue;
                }

                // set sample
                if (data[0] != 0xff) {
                    player->channels[k].sample_index = data[0];
                    player->channels[k].volume = player->module->sample_headers[data[0]].volume;
                }

                // set period (note)
                if (data[1] != 0xff) {
                    
                    player->ledstate |= (uint8_t)(1<<(uint8_t)k);
                    
                    
                    //PORTB |= (unsigned char)(1 << k);
                    // special hack for note portamento... TODO remove here
                    if (data[2] == 0x3) {
                        player->channels[k].dest_period = player_period_by_index(data[1], player->module->sample_headers[player->channels[k].sample_index].finetune);  //pgm_read_byte(protracker_periods_finetune + data[1]);
                    } else {
                        if (!(player->pattern_delay_active)) {
                            //player->channels[channel_num].period = data->period;
                            player->channels[k].period_index = data[1];
                            player->channels[k].sample_pos = 0;
                            //player_channel_set_period(player, data[1], k);
                            //player_channel_set_frequency(player, player->channels[k].period, k);
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
            /*
            if (player->current_tick == 0) {
                uart_putc_hex(player->current_pattern);
                uart_putc(' ');
                uart_putc_hex(player->current_row);
                uart_putc(' ');
                uart_putc_hex((uint8_t)k);
                uart_putc('\n');
            }
            */
            (player->effect_map)[player->channels[k].current_effect_num](player, k);
        }
        
        if (player->current_tick == 1)
            player->ledstate = 16;
        
        player->current_tick++;
        player->tick_pos = player->tick_duration;

    }

    // mixing
    

    //nano_mix_t mix;
     
    
    mix_t res;
    res.output = player_mix(player->channels, player->module->sample_headers, 4) >> 4; //(uint8_t)(mix >> 8);
    res.ledstate = player->ledstate;
    //mix.i16 = 0;
    
    
    /*
    mix = 0;
    for (k = 0; k < player->module->num_channels; k++) 
        mix += (uint16_t)(player_channel_fetch_sample( &(player->channels[k]), &(player->module->sample_headers[player->channels[k].sample_index])) * (uint16_t)player->channels[k].volume);
    */
    
    //*output_mix = (uint8_t)(mix >> 8);
    //*ledstate = player->ledstate;
    
    player->tick_pos--;
    
    return res;
    //return 2;
}
#endif

void player_init_defaults(player_t * player) 
{
    player->bpm = 125;
    player->speed = 6;
}

uint16_t player_calc_tick_duration(const uint16_t bpm, const uint16_t sample_rate) 
{
    return (((sample_rate / (bpm / 60.0)) / 4.0) / 6.0);
}

int16_t player_period_by_index(const uint8_t period_index, const int8_t finetune)
{
    return pgm_read_word_near(protracker_periods_finetune + (finetune * protracker_num_periods) + period_index);    
}

void player_channel_set_period(player_t * player, const uint8_t period_index, const int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);
    module_sample_header_t * sample_header = &(player->module->sample_headers[channel->sample_index]);

    
    player->channels[channel_num].period = pgm_read_word_near(protracker_periods_finetune + (sample_header->finetune * protracker_num_periods) + period_index);
    //player_channel_set_frequency(player, period, channel_num);
}


void player_channel_set_frequency(player_t * player, const int16_t period, const int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);

    channel->frequency =  70937892ul / (period * 20);
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