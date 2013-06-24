/*
 *  Copyright (C) RedHat inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __JTHREAD_MAP_H__
#define __JTHREAD_MAP_H__


#include "jthrowable_circular_buf.h"


/*
 * Map of TID to jthrowable_circular_buf
 */
typedef struct jthread_map T_jthreadMap;



/*
 * Initializes a new map
 *
 * @returns Mallocated memory which must be release by @jthread_map_free
 */
T_jthreadMap *jthread_map_new();



/*
 * Frees map's memory
 *
 * Doesn't release memory of stored circular buffers.
 *
 * @param map Pointer to @jthread_map. Accepts NULL
 */
void jthread_map_free(T_jthreadMap *map);



/*
 * Adds a new map item identified by @tid with value @buffer
 *
 * Does nothing if item with same @tid already exists in @map
 *
 * @param map Map
 * @param tid New item ID
 * @param buffer A pointer to @jthrowable_circular_buf
 */
void jthread_map_push(T_jthreadMap *map, jlong tid, T_jthrowableCircularBuf *buffer);



/*
 * Gets an value associated with @tid
 *
 * @param map Map
 * @param tid Required ID
 * @returns A pointer to stored @jthrowable_circular_buf or NULL if item with @tid was not found
 */
T_jthrowableCircularBuf *jthread_map_get(T_jthreadMap *map, jlong tid);



/*
 * Removes an item with ID equals to @tid from the map
 *
 * @param map Map
 * @param tid Removed item's ID
 * @returns A pointer to stored @jthrowable_circular_buf or NULL if item with @tid was not found
 */
T_jthrowableCircularBuf *jthread_map_pop(T_jthreadMap *map, jlong tid);



#endif //__JTHREAD_MAP_H__



/*
 * finito
 */
