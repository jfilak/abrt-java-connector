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
#ifndef __JTHREAD_SET_H__
#define __JTHREAD_SET_H__


#include "jthrowable_circular_buf.h"


typedef struct jthread_set T_jthreadSet;

T_jthreadSet *jthread_set_new();
void jthread_set_free(T_jthreadSet *set);

void jthread_set_push(T_jthreadSet *set, jlong tid, T_jthrowableCircularBuf *buffer);
T_jthrowableCircularBuf *jthread_set_get(T_jthreadSet *set, jlong tid);
T_jthrowableCircularBuf *jthread_set_pop(T_jthreadSet *set, jlong tid);

#endif //__JTHREAD_SET_H__



/*
 * finito
 */
