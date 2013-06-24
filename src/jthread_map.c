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
#include "jthread_map.h"
#include "abrt-checker.h"
#include "jthrowable_circular_buf.h"

#include <stdlib.h>
#include <assert.h>


/*
 * Number of elements
 */
#define MAP_SIZE 111



struct jthread_map_item;

typedef struct jthread_map_item {
    long tid;                         ///< item ID from Thread.getId()
    T_jthrowableCircularBuf *buffer;  ///< an instance of circular buffer for thread
    struct jthread_map_item *next;    ///< a next item mapped to same element
} T_jthreadMapItem;



struct jthread_map {
    T_jthreadMapItem *items[MAP_SIZE]; ///< map elements
};



T_jthreadMap *jthread_map_new()
{
    T_jthreadMap *map = (T_jthreadMap *)calloc(1, sizeof(*map));
    if (NULL == map)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": calloc() error\n");
    }

    return map;
}



void jthread_map_free(T_jthreadMap *map)
{
    if (NULL == map)
    {
        return;
    }

    free(map);
}



static T_jthreadMapItem *jthrowable_map_item_new(long tid, T_jthrowableCircularBuf *buffer)
{
    T_jthreadMapItem *itm = malloc(sizeof(*itm));
    if (NULL == itm)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": malloc(): out of memory");
        return NULL;
    }

    itm->tid = tid;
    itm->buffer = buffer;
    itm->next = NULL;
    return itm;
}



static void jthread_map_item_free(T_jthreadMapItem *itm)
{
    if (NULL == itm)
    {
        return;
    }

    free(itm);
}



void jthread_map_push(T_jthreadMap *map, jlong tid, T_jthrowableCircularBuf *buffer)
{
    assert(NULL != map);

    const long index = tid % MAP_SIZE;
    T_jthreadMapItem *last = NULL;
    T_jthreadMapItem *itm = map->items[index];
    while(NULL != itm && itm->tid != tid)
    {
        last = itm;
        itm = itm->next;
    }

    if (NULL == itm)
    {
        T_jthreadMapItem *new = jthrowable_map_item_new(tid, buffer);
        if (last == NULL)
        {
            map->items[index] = new;
        }
        else
        {
            last->next = new;
        }
    }
}



T_jthrowableCircularBuf *jthread_map_get(T_jthreadMap *map, jlong tid)
{
    assert(NULL != map);

    const size_t index = tid % MAP_SIZE;

    for (T_jthreadMapItem *itm = map->items[index]; NULL != itm; itm = itm->next)
    {
        if (itm->tid == tid)
        {
            return itm->buffer;
        }
    }

    return NULL;
}



T_jthrowableCircularBuf *jthread_map_pop(T_jthreadMap *map, jlong tid)
{
    assert(NULL != map);

    const size_t index = tid % MAP_SIZE;
    T_jthrowableCircularBuf *buffer = NULL;
    if (NULL != map->items[index])
    {
        T_jthreadMapItem *last = NULL;
        T_jthreadMapItem *itm = map->items[index];
        while (NULL != itm && itm->tid != tid)
        {
            last = itm;
            itm = itm->next;
        }

        if (NULL != itm)
        {
            buffer = itm->buffer;

            if (NULL == last)
            {
                map->items[index] = itm->next;
            }
            else
            {
                last->next = itm->next;
            }

            jthread_map_item_free(itm);
        }
    }

    return buffer;
}



/*
 * finito
 */
