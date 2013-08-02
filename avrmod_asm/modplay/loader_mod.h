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

/* MOD specific tables / structures
 */
struct modtype {
        uint32_t signature;
        uint8_t num_channels;
        char * description;
};

/* Different signatures for modules.
 * The format is the same, but different trackers use different 
 * signatures
 */
static const int loader_mod_num_modtypes = 8;
static const struct modtype loader_mod_modtypes[] = {
         { 0x2e4b2e4d, 4, "Protracker module" },                                // M.K.
         { 0x214b214d, 4, "Protracker module with more than 64 patterns" },     // M!K!
         { 0x34544c46, 4, "Startrekker, 4 channels" },                          // FLT4
         { 0x38544c46, 8, "Startrekker, 8 channels" },                          // FLT8
         { 0x4e484332, 2, "Extended protracker, 2 channels" },                  // 2CHN
         { 0x4e484334, 4, "Extended protracker, 4 channels" },                  // 4CHN
         { 0x4e484336, 6, "Extended protracker, 6 channels" },                  // 6CHN
         { 0x4e484338, 8, "Extended protracker, 8 channels" }                   // 8CHN
};


/* Prototypes
 */
int loader_mod_loadfile(module_t * module, char * filename);


#endif	/* LOADER_MOD_H */

