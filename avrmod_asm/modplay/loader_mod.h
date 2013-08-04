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

/* Prototypes
 */
void loader_mod_ls();
int loader_mod_loadfile(module_t * module, char * filename);


#endif	/* LOADER_MOD_H */

