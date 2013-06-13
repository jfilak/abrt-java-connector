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
#include "jthread_set.h"
#include "abrt-checker.h"
#include "jthrowable_circular_buf.h"

#include <stdlib.h>
#include <assert.h>



#define MAP_SIZE 111



struct jthread_map_item;

typedef struct jthread_map_item {
    long tid;
    T_jthrowableCircularBuf *buffer;
    struct jthread_map_item *next;
} T_jthreadSetItem;



struct jthread_set {
    T_jthreadSetItem *items[MAP_SIZE];
};



T_jthreadSet *jthread_set_new()
{
    T_jthreadSet *set = (T_jthreadSet *)calloc(1, sizeof(*set));
    if (NULL == set)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": calloc() error\n");
    }

    return set;
}



void jthread_set_free(T_jthreadSet *set)
{
    if (NULL == set)
    {
        return;
    }

    free(set);
}



static T_jthreadSetItem *jthrowable_set_item_new(long tid, T_jthrowableCircularBuf *buffer)
{
    T_jthreadSetItem *itm = malloc(sizeof(*itm));
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



static void jthread_set_item_free(T_jthreadSetItem *itm)
{
    if (NULL == itm)
    {
        return;
    }

    free(itm);
}



void jthread_set_push(T_jthreadSet *set, jlong tid, T_jthrowableCircularBuf *buffer)
{
    assert(NULL != set);

    const long index = tid % MAP_SIZE;
    T_jthreadSetItem *last = NULL;
    T_jthreadSetItem *itm = set->items[index];
    while(NULL != itm && itm->tid != tid)
    {
        last = itm;
        itm = itm->next;
    }

    if (NULL == itm)
    {
        T_jthreadSetItem *new = jthrowable_set_item_new(tid, buffer);
        if (last == NULL)
        {
            set->items[index] = new;
        }
        else
        {
            last->next = new;
        }
    }
}



T_jthrowableCircularBuf *jthread_set_get(T_jthreadSet *set, jlong tid)
{
    assert(NULL != set);

    const size_t index = tid % MAP_SIZE;

    for (T_jthreadSetItem *itm = set->items[index]; NULL != itm; itm = itm->next)
    {
        if (itm->tid == tid)
        {
            return itm->buffer;
        }
    }

    return NULL;
}



T_jthrowableCircularBuf *jthread_set_pop(T_jthreadSet *set, jlong tid)
{
    assert(NULL != set);

    const size_t index = tid % MAP_SIZE;
    T_jthrowableCircularBuf *buffer = NULL;
    if (NULL != set->items[index])
    {
        T_jthreadSetItem *last = NULL;
        T_jthreadSetItem *itm = set->items[index];
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
                set->items[index] = itm->next;
            }
            else
            {
                last->next = itm->next;
            }

            jthread_set_item_free(itm);
        }
    }

    return buffer;
}



/*
 * finito
 */
