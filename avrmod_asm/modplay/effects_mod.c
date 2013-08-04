/* 
 * File:   effects_mod.c
 * Author: vlo
 * 
 * Implementation of Protracker Effects
 *
 * Created on 30. Juni 2013, 00:11
 */

#include "effects_mod.h"
#include "protracker.h"

#include <stdio.h>
#include <stdlib.h>

void effects_mod_init(effect_callback_t * effect_map)
{
    int i;
    
    // initialize with default: unimplemented
    for (i = 0; i < 0xf; i++)
        effect_map[i] = effects_mod_unimplemented;
    
    effect_map[0x0] = effects_mod_0_arpeggio;
    effect_map[0x1] = effects_mod_1_slideup;
    effect_map[0x2] = effects_mod_2_slidedown;
    effect_map[0x3] = effects_mod_3_slidetonote;
    effect_map[0x4] = effects_mod_4_vibrato;
    effect_map[0x5] = effects_mod_5_slidetonote_volumeslide;
    effect_map[0x6] = effects_mod_6_vibrato_volumeslide;
    effect_map[0x7] = effects_mod_7_tremolo;
    effect_map[0x8] = effects_mod_8_panning;
    effect_map[0x9] = effects_mod_9_sampleoffset;
    effect_map[0xa] = effects_mod_a_volumeslide;
    effect_map[0xb] = effects_mod_b_positionjump;
    effect_map[0xc] = effects_mod_c_setvolume;
    effect_map[0xd] = effects_mod_d_patternbreak;
    effect_map[0xe] = effects_mod_e_special;
    effect_map[0xf] = effects_mod_f_setspeed;
    
}


void effects_mod_0_arpeggio(player_t * player, int channel_num)
{
    player_channel_t * channel = &(player->channels[channel_num]);
    
    // 000 means no effect - therefore also no arpeggio
    if (channel->current_effect_value == 0)     
        return;
    
    uint8_t temp = player->current_tick % 3;
    int temp2 = channel->period_index;//protracker_lookup_period_index(channel->period);
    
    // arpeggio without note makes no sense (bladswed.mod))
    if (!channel->period)
        return;
    
    if (temp2 == -1) {
        fprintf(stderr, "\narpeggio: error looking up period: %i\n", channel->period);
    }
    switch (temp) {
    case 0:
        //temp2 = protracker_periods_finetune[][temp2];
        break;
    case 1:
        temp2 += (channel->current_effect_value >> 4);
        break;
    case 2:
        temp2 += (channel->current_effect_value & 0x0f);
        break;
    }
    
    if (temp2 > protracker_num_periods)
        temp2 -= protracker_num_periods;
    
    player_channel_set_period(player, temp2, channel_num);
    player_channel_set_frequency(player, channel->period, channel_num);
    
}

void effects_mod_1_slideup(player_t * player, int channel) 
{
    if (player->current_tick == 0)
        return;
    player->channels[channel].period -= player->channels[channel].current_effect_value;
    if (player->channels[channel].period < PROTRACKER_PERIOD_MIN)
        player->channels[channel].period = PROTRACKER_PERIOD_MIN;
    player_channel_set_frequency(player, player->channels[channel].period, channel);
}

void effects_mod_2_slidedown(player_t * player, int channel) 
{
    if (player->current_tick == 0)
        return;
    
    player->channels[channel].period += (player->channels[channel].current_effect_value);
    if (player->channels[channel].period > PROTRACKER_PERIOD_MAX)
        player->channels[channel].period = PROTRACKER_PERIOD_MAX;
    player_channel_set_frequency(player, player->channels[channel].period, channel);    
}

void effects_mod_3_slidetonote(player_t * player, int channel)
{
    if (player->current_tick == 0)
        return;
    
    if (player->channels[channel].dest_period == 0)
        return;
    
    if ((player->channels[channel].current_effect_value) != 0x00) 
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] = player->channels[channel].current_effect_value;
    
    if (player->channels[channel].period > player->channels[channel].dest_period) {
        int tmp = (int)player->channels[channel].period - (int)player->channels[channel].effect_last_value[player->channels[channel].current_effect_num];
        if (tmp < player->channels[channel].dest_period)
            tmp = player->channels[channel].dest_period;
        player->channels[channel].period = (uint16_t)tmp;
    } else if (player->channels[channel].period < player->channels[channel].dest_period) {
        player->channels[channel].period += player->channels[channel].effect_last_value[player->channels[channel].current_effect_num];
        if (player->channels[channel].period > player->channels[channel].dest_period)
            player->channels[channel].period = player->channels[channel].dest_period;
    }
    player_channel_set_frequency(player, player->channels[channel].period, channel);
}

void effects_mod_4_vibrato(player_t * player, int channel) 
{
    uint8_t temp;
    uint16_t delta;
    
    if (player->current_tick == 0)
        return;    
    
    if ((player->channels[channel].current_effect_value >> 4) != 0x00) {
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] &= 0x0f;
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] |= (player->channels[channel].current_effect_value & 0xf0);
    }

    if ((player->channels[channel].current_effect_value & 0xf) != 0x00) {
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] &= 0xf0;
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] |= (player->channels[channel].current_effect_value & 0x0f);
    }

    temp = player->channels[channel].vibrato_state & 0x1f;
    delta = pgm_read_byte_near(protracker_sine_table + temp);
    
    delta *= player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] & 0x0f;
    delta /= 128;
    
    if (player->channels[channel].vibrato_state >= 0)
        player_channel_set_frequency(player, player->channels[channel].period + delta, channel);
    else
        player_channel_set_frequency(player, player->channels[channel].period - delta, channel);
    
    player->channels[channel].vibrato_state += (player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] >> 4);
    if (player->channels[channel].vibrato_state > 31)
        player->channels[channel].vibrato_state -= 64;
}

void effects_mod_5_slidetonote_volumeslide(player_t * player, int channel) 
{
    if (player->current_tick == 0)
        return;
    
    // maintain portamento based on last portamento to note effect value
    if (player->channels[channel].period > player->channels[channel].dest_period) {
        int tmp = (int)player->channels[channel].period - (int)player->channels[channel].effect_last_value[0x3];
        if (tmp < player->channels[channel].dest_period)
            tmp = player->channels[channel].dest_period;
        player->channels[channel].period = (uint16_t)tmp;
    } else if (player->channels[channel].period < player->channels[channel].dest_period) {
        player->channels[channel].period += player->channels[channel].effect_last_value[0x3];
        if (player->channels[channel].period > player->channels[channel].dest_period)
            player->channels[channel].period = player->channels[channel].dest_period;
    }
    player_channel_set_frequency(player, player->channels[channel].period, channel);
    
    // do volume slide based on current effect values
    if (player->channels[channel].current_effect_value & 0xf0) {
        player->channels[channel].volume += (player->channels[channel].current_effect_value & 0xf0) >> 4;
        if (player->channels[channel].volume > 64)
            player->channels[channel].volume = 64;
    } else {
        int tmp = (int)player->channels[channel].volume - (int)(player->channels[channel].current_effect_value & 0x0f);
        if (tmp < 0)	
            tmp = 0;
        player->channels[channel].volume = (uint8_t)tmp;
    }
        
}

void effects_mod_6_vibrato_volumeslide(player_t * player, int channel) 
{
    
    uint8_t temp;
    uint16_t delta;
    
    if (player->current_tick == 0)
        return;    
    
    // maintain vibrato using last vibrato effect params
    temp = player->channels[channel].vibrato_state & (uint8_t)0x1f;
    delta = pgm_read_byte_near(protracker_sine_table + temp);
    
    delta *= player->channels[channel].effect_last_value[0x4] & (uint8_t)0x0f;
    delta /= 128;
    
    if (player->channels[channel].vibrato_state >= 0)
        player_channel_set_frequency(player, player->channels[channel].period + delta, channel);
    else
        player_channel_set_frequency(player, player->channels[channel].period - delta, channel);

    player->channels[channel].vibrato_state += player->channels[channel].effect_last_value[0x4] >> 4;
    if (player->channels[channel].vibrato_state > 31)
        player->channels[channel].vibrato_state -= 64;    
    
    // do volume slide based on current effect values
    if (player->channels[channel].current_effect_value & 0xf0) {
        player->channels[channel].volume += (player->channels[channel].current_effect_value >> 4);
        if (player->channels[channel].volume > 64)
            player->channels[channel].volume = 64;
    } else {
        int tmp = (int)player->channels[channel].volume - (int)(player->channels[channel].current_effect_value & 0x0f);
        if (tmp < 0)	
            tmp = 0;
        player->channels[channel].volume = (uint8_t)tmp;
    }      
}

void effects_mod_7_tremolo(player_t * player, int channel) 
{
    uint8_t temp;
    int temp2;
    uint16_t delta;
    
    if (player->current_tick == 0) 
        return;    
    
    
    if ((player->channels[channel].current_effect_value >> 4) != 0x00) {
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] &= 0x0f;
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] |= (player->channels[channel].current_effect_value & 0xf0);
    }

    if ((player->channels[channel].current_effect_value & 0xf) != 0x00) {
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] &= 0xf0;
        player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] |= (player->channels[channel].current_effect_value & 0x0f);
    }

    temp = player->channels[channel].tremolo_state & (uint8_t)0x1f;
    delta = pgm_read_byte_near(protracker_sine_table + temp);
    
    delta *= player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] & 0x0f;
    delta /= 64;
    
    if (player->channels[channel].tremolo_state >= 0) {
        if (player->channels[channel].volume + delta > 64) 
            delta = 64 - player->channels[channel].volume;
        temp2 = (int)player->channels[channel].volume + delta;
        player->channels[channel].volume = (uint8_t)temp2;
    } else {
        if ((int)player->channels[channel].volume - delta < 0) 
            delta = player->channels[channel].volume;

        temp2 = (int)player->channels[channel].volume - delta;
        player->channels[channel].volume = (uint8_t)temp2;
    }
    
    //fprintf(stderr, "tremolo: s=%i, v=%i\n", player->channels[channel].tremolo_state, player->channels[channel].volume_master);
    
    player->channels[channel].tremolo_state += player->channels[channel].effect_last_value[player->channels[channel].current_effect_num] >> 4;
    if (player->channels[channel].tremolo_state > 31)
        player->channels[channel].tremolo_state -= 64;
}

void effects_mod_8_panning(player_t * player, int channel)
{
    player->channels[channel].panning = player->channels[channel].current_effect_value;
}

void effects_mod_9_sampleoffset(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        player->channels[channel].sample_pos = ((uint32_t)((((player->channels[channel].current_effect_value & 0xf0) >> 4) * 4096) + (player->channels[channel].current_effect_value & 0xf) * 256) << 8);
    }
}

void effects_mod_a_volumeslide(player_t * player, int channel)
{
    if (player->current_tick == 0)
        return;
    
    if (player->channels[channel].current_effect_value & 0xf0) {
        player->channels[channel].volume += (player->channels[channel].current_effect_value & 0xf0) >> 4;
        if (player->channels[channel].volume > 64)
            player->channels[channel].volume = 64;
    } else {
        int tmp = (int)player->channels[channel].volume - (int)(player->channels[channel].current_effect_value & 0x0f);
        if (tmp < 0)	
            tmp = 0;
        player->channels[channel].volume = (uint8_t)tmp;
    }
}

void effects_mod_b_positionjump(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        player->next_order = player->channels[channel].current_effect_value;
        player->next_row = 0;
        player->do_break = 1;
    }
}

void effects_mod_c_setvolume(player_t * player, int channel)
{
    if (player->current_tick == 0)
        player->channels[channel].volume = player->channels[channel].current_effect_value;
}

void effects_mod_d_patternbreak(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        player->next_row = (((player->channels[channel].current_effect_value & 0xf0) >> 4) * 10 + (player->channels[channel].current_effect_value & 0x0f));
        player->do_break = 1;
    }    
}

void effects_mod_e_special(player_t * player, int channel)
{
    // dispatch special commands
    switch (player->channels[channel].current_effect_value >> 4) {
        case 0x1: effects_mod_e1_fineslideup(player, channel); break;
        case 0x2: effects_mod_e2_fineslidedown(player, channel); break;
        case 0x6: effects_mod_e6_patternloop(player, channel); break;
        case 0x8: effects_mod_e8_panning(player, channel); break;
        case 0x9: effects_mod_e9_retriggersample(player, channel); break;
        case 0xa: effects_mod_ea_finevolumeup(player, channel); break;
        case 0xb: effects_mod_eb_finevolumedown(player, channel); break;
        case 0xc: effects_mod_ec_notecut(player, channel); break;
        case 0xd: effects_mod_ed_delaysample(player, channel); break;
        case 0xe: effects_mod_ee_patterndelay(player, channel); break;
        default: effects_mod_unimplemented(player, channel); break;
    }
}

void effects_mod_e6_patternloop(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        if ((player->channels[channel].current_effect_value & 0x0f) == 0) {// set pos
            player->channels[channel].pattern_loop_position = player->current_row;
        } else {
            if (player->channels[channel].pattern_loop_count < (player->channels[channel].current_effect_value & 0x0f)) {
                player->next_row = player->channels[channel].pattern_loop_position;
                player->channels[channel].pattern_loop_count++;
            } else {
                player->channels[channel].pattern_loop_count = 0;
            }
        }
    }    
}

void effects_mod_e1_fineslideup(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        player->channels[channel].period -= player->channels[channel].current_effect_value & 0xf;
        if (player->channels[channel].period < PROTRACKER_PERIOD_MIN)
            player->channels[channel].period = PROTRACKER_PERIOD_MIN;
        player_channel_set_frequency(player, player->channels[channel].period, channel);
    }
}

void effects_mod_e2_fineslidedown(player_t * player, int channel) 
{
    if (player->current_tick == 0) {
        player->channels[channel].period += (player->channels[channel].current_effect_value & 0xf);
        if (player->channels[channel].period > PROTRACKER_PERIOD_MAX)
            player->channels[channel].period = PROTRACKER_PERIOD_MAX;
        player_channel_set_frequency(player, player->channels[channel].period, channel);    
    }
}

void effects_mod_e8_panning(player_t * player, int channel)
{
    player->channels[channel].panning = player->channels[channel].current_effect_value + 0x10;
}

void effects_mod_e9_retriggersample(player_t * player, int channel)
{
    if ((player->channels[channel].current_effect_value & 0xf) == 0)
        return;
    
    if ((player->current_tick % (player->channels[channel].current_effect_value & 0xf)) == 0)
        player->channels[channel].sample_pos = 0;
}

void effects_mod_ea_finevolumeup(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        player->channels[channel].volume += (player->channels[channel].current_effect_value & 0xf);
        if (player->channels[channel].volume > 0x40)
            player->channels[channel].volume = 0x40;
    }
}

void effects_mod_eb_finevolumedown(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        int tmp = player->channels[channel].volume - (player->channels[channel].current_effect_value & 0xf);
        if (tmp < 0)
            tmp = 0;
        player->channels[channel].volume = tmp;
    }    
}

void effects_mod_ec_notecut(player_t * player, int channel)
{
    if ((player->channels[channel].current_effect_value & 0x0f) <= player->current_tick)
        player->channels[channel].volume = 0;
}

void effects_mod_ed_delaysample(player_t * player, int channel)
{
    if (player->current_tick == 0)
        player->channels[channel].sample_delay = 0;
    
    if (player->channels[channel].sample_delay == (player->channels[channel].current_effect_value & 0xf)) {
        if (player->channels[channel].dest_sample_index != 0xff) {
            player->channels[channel].sample_index = player->channels[channel].dest_sample_index;
            player->channels[channel].volume = player->module->sample_headers[player->channels[channel].sample_index].volume;
        }
        
        if (player->channels[channel].dest_period > 0) {
            player->channels[channel].period = player->channels[channel].dest_period;
            player->channels[channel].sample_pos = 0;
            player_channel_set_frequency(player, player->channels[channel].period, channel);
        }            
    }
    
    player->channels[channel].sample_delay++; // = (player->channels[channel].current_effect_value & 0xf);
}

void effects_mod_ee_patterndelay(player_t * player, int channel) 
{
    if (!player->pattern_delay_active)
        player->pattern_delay = (player->channels[channel].current_effect_value & 0x0f);
}

void effects_mod_f_setspeed(player_t * player, int channel)
{
    if (player->current_tick == 0) {
        if (player->channels[channel].current_effect_value <= 32) {
            player->speed = player->channels[channel].current_effect_value;
        } else {
            player->bpm = player->channels[channel].current_effect_value;
            player->tick_duration = player_calc_tick_duration(player->bpm, player->sample_rate);
        }
    }    
}

void effects_mod_unimplemented(player_t * player, int channel)
{
    // only alert once per row
 
 
}