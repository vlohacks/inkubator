/* 
 * File:   loader_mod.h
 * Author: vlo
 * 
 * Loader routines for .mod files
 *
 * Created on 29. Juni 2013, 13:37
 */

#ifndef LOADER_MOD_H
#define	LOADER_MOD_H

#include "module.h"
#include "../mmc/fat.h"

/* Prototypes
 */
void loader_mod_ls();

uint16_t loader_mod_get_song_count();
uint8_t loader_mod_load_song_by_number(module_t * module, uint16_t song_num);
uint8_t loader_mod_load_song_by_filename(module_t * module, char * filename);

int loader_mod_loadfile(module_t * module, struct fat_file_struct * fd);


#endif	/* LOADER_MOD_H */

