/* 
 * File:   loader_mod.c
 * Author: vlo
 * 
 * Loader routines for .mod files
 *
 * Created on 29. Juni 2013, 13:57
 */
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <stdio.h>
#include "../mmc/fat.h"
#include "../mmc/partition.h"
#include "../mmc/sd_raw.h"
#include "../mmc/uart.h"

#include "loader_mod.h"
#include "protracker.h"
#include "arch.h"
#include "../sram.h"

#define DEBUG 1

static struct partition_struct * partition;
static struct fat_fs_struct * fs;
static struct fat_dir_struct * dd;
static struct fat_dir_entry_struct * directory;

uint8_t find_file_in_dir(struct fat_fs_struct * fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct * dir_entry)
{
    while(fat_read_dir(dd, dir_entry)) {
        if(strcmp(dir_entry->long_name, name) == 0) {
            fat_reset_dir(dd);
            return 1;
        }
    }

    return 0;
}

struct fat_file_struct * open_file_in_dir(struct fat_fs_struct * fs, struct fat_dir_struct* dd, const char* name)
{
    struct fat_dir_entry_struct file_entry;
    if(!find_file_in_dir(fs, dd, name, &file_entry))
        return 0;

    return fat_open_file(fs, &file_entry);
}


void loader_mod_cleanup_sd()
{
    if (directory)
        free(directory);
        
    fat_close_dir(dd);
    fat_close(fs);
    partition_close(partition);
    
    raw_block_free();    
}

uint8_t loader_mod_setup_sd() 
{
    uint8_t error;
    raw_block_alloc();
    
    error = 0;
    
    if(!sd_raw_init()) {
#if DEBUG
        uart_puts_p(PSTR("MMC/SD initialization failed\n"));
#endif
        error = 1;
    }
    
    directory = (struct fat_dir_entry_struct *) malloc(sizeof(struct fat_dir_entry_struct));
    
    partition = partition_open(sd_raw_read, sd_raw_read_interval, 0, 0, 0);
    
    if(!partition) {
         partition = partition_open(sd_raw_read, sd_raw_read_interval, 0, 0, -1);
         if(!partition) {
#if DEBUG
             uart_puts_p(PSTR("opening partition failed\n"));
#endif
             error = 1;
         }
    }
    
    /* open file system */
    fs = fat_open(partition);    
    if(!fs) {
#if DEBUG
        uart_puts_p(PSTR("opening filesystem failed\n"));
#endif
        error = 1;
    }    
    

    /* open root directory */
    fat_get_dir_entry_of_path(fs, "/", directory);

    dd = fat_open_dir(fs, directory);
    if(!dd) {
#if DEBUG
        uart_puts_p(PSTR("opening root directory failed\n"));
#endif
        error = 1;
    }
    
    if (error)
        loader_mod_cleanup_sd();
    
    return error;
        
}





void loader_mod_ls() 
{
    struct fat_dir_entry_struct dir_entry;

    if (loader_mod_setup_sd())
        return;

    while(fat_read_dir(dd, &dir_entry))
    {
        uint8_t spaces = sizeof(dir_entry.long_name) - strlen(dir_entry.long_name) + 4;

        uart_puts(dir_entry.long_name);
        uart_putc(dir_entry.attributes & FAT_ATTRIB_DIR ? '/' : ' ');
        while(spaces--)
            uart_putc(' ');
        uart_putdw_dec(dir_entry.file_size);
        uart_putc('\n');
    }    
    
    loader_mod_cleanup_sd();
   
}

uint16_t loader_mod_get_song_count() 
{
    
    struct fat_dir_entry_struct dir_entry;
    
    if (loader_mod_setup_sd())
        return 0;
    
    uint16_t count = 0;
    
    while(fat_read_dir(dd, &dir_entry))
    {
        count ++;
        if (count == 0xffff)
            break;
    }        
    
    loader_mod_cleanup_sd();
    return count;
}

uint8_t loader_mod_load_song_by_number(module_t * module, uint16_t song_num) 
{
    uint16_t count = 0;
    
    struct fat_dir_entry_struct file_entry;
    struct fat_file_struct * fd = 0;
    
    if (loader_mod_setup_sd())
        return 1;
    
    
    uart_putw_dec(song_num);
    uart_putc('\n');
    
    fat_reset_dir(dd);
    while(fat_read_dir(dd, &file_entry)) 
    {
        if (count == song_num) {
            
            fat_reset_dir(dd);
            
            uart_putw_dec(count);
            uart_puts(file_entry.long_name);
            uart_putc('\n');
            
            fd = fat_open_file(fs, &file_entry);

            break;
        }
        count ++;
    }  
    
    
    if(!fd) {
        uart_puts_p(PSTR("error opening file\n"));
        return 1;
    }            
    
    loader_mod_loadfile(module, fd);
    
    fat_close_file(fd);    
    
    loader_mod_cleanup_sd();
    
    return 0;
    
}

uint8_t loader_mod_load_song_by_filename(module_t * module, char * filename) 
{
    uint8_t error = 0;
    
    if (loader_mod_setup_sd())
        return 1;

    struct fat_file_struct * fd;
    
    fd = open_file_in_dir(fs, dd, filename);
    if(!fd) {
        uart_puts_p(PSTR("error opening "));
        uart_puts(filename);
        uart_putc('\n');
        error = 1;
    }
    
    if (!error)
        loader_mod_loadfile(module, fd);
    
    fat_close_file(fd);
    loader_mod_cleanup_sd();

    return error;
}

/* Loads a protracker/startrekker/soundtracker module file (*.mod, *.stk)
 */
int loader_mod_loadfile(module_t * module, struct fat_file_struct * fd)
{
    int i, k, r;
    int8_t tmp8;
    uint16_t tmp16;
    uint32_t sram_addr = 0;
    uint8_t tmp[32];
    int32_t tmp32;
    
    // Probe for standard MOD types
    
    tmp32 = 0x438;
    fat_seek_file(fd, &tmp32, SEEK_SET);
    fat_read_file(fd, tmp, 4);
    tmp[4] = 0;

    uart_puts_p(PSTR("Sig: "));
    uart_puts((const char *)tmp);
    uart_putc('\n'); 
    
    
    module->num_samples = 31;
    module->num_channels = 4;
        
    // determine MOD type by reading signature
    
    if (!strcmp(tmp, "M.K.") || !strcmp(tmp, "M!K!") || !strcmp(tmp, "FLT4")) {
        module->num_channels = 4;
    } else if (!strcmp(tmp, "6CHN")) {
        module->num_channels = 6;
    } else if (!strcmp(tmp, "8CHN")) {
        module->num_channels = 8;
    } else {
        // no signature, assume Soundtracker
        module->num_channels = 4;
        module->num_samples = 15;
    }
    
    
    uart_puts_p(PSTR("Channels: "));
    uart_putc_hex(module->num_channels);
    uart_putc('\n');

    uart_puts_p(PSTR("Samples: "));
    uart_putc_hex(module->num_samples);
    uart_putc('\n');
    
    // now we know what kind of MOD it is, so we can start with REAL loading
    tmp32 = 0;
    fat_seek_file(fd, &tmp32, SEEK_SET);
    
    r = fat_read_file(fd, tmp, 20);
    tmp[20] = 0;
    uart_puts_p(PSTR("Song: "));
    uart_puts((const char *)tmp);
    uart_putc('\n');
    
    
    uart_puts_p(PSTR("Load sample header data...\n"));

    // load sample header data (aka song message :-))
    for (i = 0; i < 31; i++) {
        
        module_sample_header_t * hdr = &(module->sample_headers[i]);
        
        if (i < module->num_samples) {
            r = fat_read_file(fd, tmp, 22);
            tmp[22] = 0;
            uart_puts((const char *)tmp);
            uart_putc(' ');    

            r = fat_read_file(fd, (uint8_t *)&tmp16, 2);
            hdr->length = ((uint32_t)swap_endian_u16(tmp16)) << 1;
            uart_putdw_dec(hdr->length);
            uart_putc(' ');    

            r = fat_read_file(fd, (uint8_t *)&tmp8, 1);
            hdr->finetune = tmp8 & 0xf; 
            uart_putc_hex(hdr->finetune);
            uart_putc(' ');    

            r = fat_read_file(fd, (uint8_t *)&tmp8, 1);
            hdr->volume = tmp8;
            uart_putc_hex(hdr->volume);
            uart_putc(' ');    

            r = fat_read_file(fd, (uint8_t *)&tmp16, 2);
            hdr->loop_start = ((uint32_t)swap_endian_u16(tmp16)) << 1;
            uart_putdw_dec(hdr->loop_start);
            uart_putc(' ');    

            r = fat_read_file(fd, (uint8_t *)&tmp16, 2);
            hdr->loop_length = ((uint32_t)swap_endian_u16(tmp16)) << 1;
            hdr->loop_length += hdr->loop_start;    // in player we need to deal with the absolute pos
            uart_putdw_dec(hdr->loop_length);
            uart_putc('\n');    
        } else {
            hdr->length = 0;
        }
        
    }

    // read number of orders in mod
    r = fat_read_file(fd, (uint8_t *)&tmp8, 1);
    module->num_orders = tmp8;
    
    // read not used "load patterns" / "loop position" / whatever
    r = fat_read_file(fd, (uint8_t *)&tmp8, 1);


    // read order list
    r = fat_read_file(fd, module->orders, 128);
    
    // read signature again, just to move the filepointer - and only if the
    // file is not a STK not having a signature

    if (module->num_samples != 15) {
        r = fat_read_file(fd, tmp, 4);
        tmp[4] = 0;
        uart_puts_p(PSTR("Sig: "));
        uart_puts((const char *)tmp);
        uart_putc('\n');    
    }
    
    // determine number of patterns im MOD by fetching highest entry in orders
    module->num_patterns = 0;
    for (i = 0;i < module->num_orders; i++) {
        if (module->orders[i] > module->num_patterns)
            module->num_patterns = module->orders[i];
    }
    module->num_patterns++;

    // load pattern data to sram
    uart_puts_p(PSTR("Load pattern data...\n"));
    for (i = 0; i < module->num_patterns * 64 * module->num_channels; i++) {
        r = fat_read_file(fd, tmp, 4);
        
        tmp8 = 0;
        tmp16 = 0;
        
        /*
        if (sram_addr <= 0x100) {
            uart_putdw_hex(sram_addr);
            uart_putc('\n');            
            uart_putc_hex(tmp[0]);
            uart_putc_hex(tmp[1]);
            uart_putc_hex(tmp[2]);
            uart_putc_hex(tmp[3]);
            uart_putc('\n');
        }
        */
        
        tmp8 = ((tmp[0] & (uint8_t)0xf0) | (uint8_t)(tmp[2] >> 4));
        if (tmp8 > 0)
            tmp8--;
        else
            tmp8 = -1;
        
        //if (sram_addr <= 0x100)
        //    uart_putc_hex(tmp8);
        
        sram_write_char_i(sram_addr, tmp8);
        sram_addr++;
        
        tmp16 = ((uint16_t)(tmp[0] & (uint8_t)0x0f) << 8) | (uint16_t)(tmp[1]);
        tmp8 = protracker_lookup_period_index(tmp16);
        sram_write_char_i(sram_addr, tmp8);

        //if (sram_addr <= 0x100)
        //    uart_putc_hex(tmp8);
        
        sram_addr++;
        
        tmp8 = tmp[2] & (uint8_t)0x0f;
        
        //if (sram_addr <= 0x100)
        //    uart_putc_hex(tmp8);
        
        
        sram_write_char_i(sram_addr, tmp8);
        sram_addr++;
        
        /*
        if (sram_addr < 0x100) {
                uart_putc_hex(tmp[3]);
                uart_putc('\n');
        }
        */
        
        sram_write_char_i(sram_addr, tmp[3]);
        

        sram_addr++;
    }
    
    module->sample_headers[0].sram_offset = sram_addr;
    /*
    module->sample_headers[0].loop_start += sram_addr;
    if (module->sample_headers[0].loop_length > 2)
        module->sample_headers[0].loop_length += sram_addr;
    module->sample_headers[0].length += sram_addr;
    */
    
    for (i = 1; i < module->num_samples; i++) {
        module->sample_headers[i].sram_offset = module->sample_headers[i - 1].sram_offset + module->sample_headers[i - 1].length;
        /*
        module->sample_headers[i].loop_start += module->sample_headers[i].sram_offset;
        if (module->sample_headers[i].loop_length > 2)
                module->sample_headers[i].loop_length += module->sample_headers[i].sram_offset;
        module->sample_headers[i].length += module->sample_headers[i].sram_offset;
        */
    }

    for (i = 0; i < module->num_samples; i++) {
        module->sample_headers[i].loop_start += module->sample_headers[i].sram_offset;
        if (module->sample_headers[i].loop_length > 2)
            module->sample_headers[i].loop_length += module->sample_headers[i].sram_offset;
        if (module->sample_headers[i].length > 2)
            module->sample_headers[i].length += module->sample_headers[i].sram_offset;
    }
    
    uart_puts_p(PSTR("Load sample data...\n"));
    // load sample pcm data
    /*
    for (i = 0; i < module->num_samples; i++) {
        for (j = 0; j < module->sample_headers[i].length; j++) {
            r = fat_read_file(fd, (uint8_t *)&tmp, 32);

            //tmp8 = (uint8_t)(((int16_t)tmp8) + 128);
            for (k=0; k<r; k++) {
                tmp8 = tmp[k];
                tmp8 += 128;
                sram_write_char_i(sram_addr, tmp8);
                sram_addr++;
            }
        }
    }
    */
    
    while((r = fat_read_file(fd, (uint8_t *)&tmp, 32)) > 0) {
        for (k=0; k<r; k++) 
            sram_write_char_i(sram_addr++, tmp[k]);
        
    }
    r = 0;

    return 0;

}

