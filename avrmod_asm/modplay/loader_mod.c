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
#include "../mmc/fat.h"
#include "../mmc/partition.h"
#include "../mmc/sd_raw.h"
#include "../mmc/uart.h"

#include "loader_mod.h"
#include "protracker.h"
#include "arch.h"


uint8_t find_file_in_dir(struct fat_fs_struct * fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry)
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


/* Loads a protracker/startrekker/soundtracker module file (*.mod, *.stk)
 */
int loader_mod_loadfile(module_t * module, char * filename)
{
    int i, j, k, r;
    uint8_t tmp8;
    uint16_t tmp16;
    uint32_t sram_addr = 0;
    char tmp[32];
    
    char * nr = PSTR("\r\n");
    
    struct partition_struct * partition;
    struct fat_fs_struct * fs;
    struct fat_dir_entry_struct directory;
    struct fat_dir_struct * dd;
    uint32_t signature;
    struct fat_file_struct* fd;
    
    
    /* setup sd card slot */
    if(!sd_raw_init()) {
#if DEBUG
        uart_puts_p(PSTR("MMC/SD initialization failed\n"));
#endif
        return 1;
    }
    
    partition = partition_open(
        sd_raw_read,
        sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
        sd_raw_write,
        sd_raw_write_interval,
#else
        0,
        0,
#endif
        0
    );
    
    if(!partition) {
         /* If the partition did not open, assume the storage device
          * is a "superfloppy", i.e. has no MBR.
          */
         partition = partition_open(
             sd_raw_read,
             sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
             sd_raw_write,
             sd_raw_write_interval,
#else
             0,
             0,
#endif
             -1
         );
         if(!partition) {
#if DEBUG
             uart_puts_p(PSTR("opening partition failed\n"));
#endif
             return 1;
         }
    }
    
    /* open file system */
    fs = fat_open(partition);    
    if(!fs) {
#if DEBUG
        uart_puts_p(PSTR("opening filesystem failed\n"));
#endif
        return 1;
    }    
    

    /* open root directory */
    fat_get_dir_entry_of_path(fs, "/", &directory);

    dd = fat_open_dir(fs, &directory);
    if(!dd) {
#if DEBUG
        uart_puts_p(PSTR("opening root directory failed\n"));
#endif
        return 1;
    }    
    
    
    fd = open_file_in_dir(fs, dd, filename);
    if(!fd) {
        uart_puts_p(PSTR("error opening "));
        uart_puts(filename);
        uart_putc('\n');
        return 1;
    }    
    
    

    // Probe for standard MOD types
    module->num_samples = 31;



    // now we know what kind of MOD it is, so we can start with REAL loading
    r = fat_read_file(fd, tmp, 20);
    uart_puts_p(PSTR("Song: "));
    tmp[20] = 0;
    uart_puts(tmp);
    uart_putc('\n');    

    // load sample header data (aka song message :-))
    for (i = 0; i < module->num_samples; i++) {
        
        module_sample_header_t hdr = &(module->sample_headers[i]);
        
        r = fat_read_file(fd, tmp, 22);
        tmp[22] = 0;
        uart_puts(tmp);
        uart_putc(' ');    

        r = fat_read_file(fd, &tmp16, 2);
        hdr->length = (uint32_t)swap_endian_u16(tmp16) << 1;
        uart_putdw_dec(hdr->length);
        uart_putc(' ');    

        r = fat_read_file(fd, &tmp8, 1);
        hdr->finetune = tmp8 & 0xf; 
        uart_putc_hex(hdr->finetune);
        uart_putc(' ');    

        r = fat_read_file(fd, &tmp8, 1);
        hdr->volume = tmp8;
        uart_putc_hex(hdr->volume);
        uart_putc(' ');    

        r = fread (&word, 2, 1, f);
        if (r != 1)
            return 1;
        hdr->loop_start = swap_endian_u16(word) << 1;

        r = fread (&word, 2, 1, f);
        if (r != 1)
            return 1;
        hdr->loop_length = swap_endian_u16(word) << 1;
        
        
    }

    // read number of orders in mod
    r = fread(&tmp8, 1, 1, f);
    if (r != 1) {
        free(module);
        fclose(f);
        return 0;
    }

    module->num_orders = (uint16_t)tmp8;
    
    // read not used "load patterns" / "loop position" / whatever
    r = fread(&tmp8, 1, 1, f);
    if (r != 1) {
        free(module);
        fclose(f);
        return 0;
    }

    // read order list
    r = fread(&(module->orders), 1, 128, f);
    if (r != 128) {
        free(module);
        fclose(f);
        return 0;
    }
    
    // read signature again, just to move the filepointer - and only if the
    // file is not a STK not having a signature
    if (module->num_samples > 15) {
        r = fread(&signature, 4, 1, f);
        if (r != 1) {
            free(module);
            fclose(f);
            return 0;
        }
    }
    
    // determine number of patterns im MOD by fetching highest entry in orders
    module->num_patterns = 0;
    for (i = 0;i < module->num_orders; i++) {
        if (module->orders[i] > module->num_patterns)
            module->num_patterns = module->orders[i];
    }
    module->num_patterns++;

    // load pattern data - TODO: special FLT8 arrangement - currently broken
    module->patterns = (module_pattern_t *)malloc(sizeof(module_pattern_t) * module->num_patterns);
    for (i = 0; i < module->num_patterns; i++) {
        module->patterns[i].num_rows = 64;        // mod alwas has 64 rows per pattern
        module->patterns[i].rows = (module_pattern_row_t *)malloc(sizeof(module_pattern_row_t) * module->patterns[i].num_rows);
        for (j = 0; j < module->patterns[i].num_rows; j++) {
            module->patterns[i].rows[j].data = (module_pattern_data_t *)malloc(sizeof(module_pattern_data_t) * module->num_channels);
            for (k = 0; k < module->num_channels; k++) {
                if(loader_mod_read_pattern_data (&(module->patterns[i].rows[j].data[k]), f)) {
                    if (module->patterns[i].rows[j].data[k].period_index == -1) {
                        fprintf(stderr, "Loader: WARNING: Non-standard period in pattern: %i, channel: %i, row: %i\n", i, j, k);
                    }                    
                    free(module);
                    fclose(f);
                    return 0;
                }
            }
        }
    }

    // load sample pcm data
    for (i = 0; i < module->num_samples; i++) 
        loader_mod_read_sample_data(&(module->samples[i]), f);
	
    fclose(f);
    
    return module;
}


int loader_mod_read_sample_header(module_sample_header_t * hdr, FILE * f) 
{
    uint16_t word;
    uint8_t byte;
    int8_t sbyte;
    int r;

    r = fread (&(hdr->name), 1, 22, f);
    if (r != 22)
        return 1;
    hdr->name[22] = 0;

    r = fread (&word, 2, 1, f);
    if (r != 1)
        return 1;
    hdr->length = swap_endian_u16(word) << 1;

    r = fread (&sbyte, 1, 1, f);
    if (r != 1)
        return 1;
    hdr->finetune = sbyte & 0xf; // > 7 ? -(16-sbyte) : sbyte;

    r = fread (&byte, 1, 1, f);
    if (r != 1)
        return 1;
    hdr->volume = byte;
    
    r = fread (&word, 2, 1, f);
    if (r != 1)
        return 1;
    hdr->loop_start = swap_endian_u16(word) << 1;
    
    r = fread (&word, 2, 1, f);
    if (r != 1)
        return 1;
    hdr->loop_length = swap_endian_u16(word) << 1;
    
    return 0;
}

int loader_mod_read_pattern_data(module_pattern_data_t * data, FILE * f) 
{
    uint32_t dw;
    int r;

    r = fread(&dw, 4, 1, f);
    if (r != 1)
        return 1;

    dw = swap_endian_u32(dw);

    // AWFUL Amiga SHIT (piss doch drauf... scheiß doch rein... zünd ihn an...)
    data->sample_num = ((uint8_t)(dw >> 24) & 0xf0) | ((uint8_t)(dw >> 12) & 0x0f);
    data->period = (uint16_t)((dw >> 16) & 0x0fff);
    data->period_index = protracker_lookup_period_index(data->period);
    data->effect_num = (uint8_t)((dw & 0x0f00) >> 8);
    data->effect_value = (uint8_t)(dw & 0xff);
    
    return 0;
}

void loader_mod_read_sample_data(module_sample_t * sample, FILE * f)
{
    int i;
    int8_t * p;

    if (sample->header.length == 0) {
        sample->data = NULL;
        return;
    }

    sample->data = (int8_t *)malloc(sizeof(int8_t) * sample->header.length); //sample->header.length);
    p = sample->data;
    for (i = 0; i < sample->header.length; i++)
        *p++ = fgetc(f);
}