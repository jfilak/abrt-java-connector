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
#ifndef __JTHROWABLE_STASH__
#define __JTHROWABLE_STASH__



/*
 * JNI types
 */
#include <jni.h>



/*
 * An opaque structure representing a buffer of jthrowable objects.
 * It is a kind of set and FIFO cache.
 */
typedef struct jthrowable_circular_buf T_jthrowableCircularBuf;



/*
 * Initializes a new instance of buffer
 *
 * Result must be free by @jthrowable_circular_buf_free
 *
 * @param jni_env JNIEnv for global reference handling
 * @param capacity A maximal number of stored exceptions
 * @returns Mallocated buffer on success; otherwise NULL
 */
T_jthrowableCircularBuf *jthrowable_circular_buf_new(JNIEnv *jni_env, size_t capacity);



/*
 * Frees buffer's memory
 *
 * @param buffer A freed buffer. Can be NULL
 */
void jthrowable_circular_buf_free(T_jthrowableCircularBuf *buffer);



/*
 * Pushes a new exception object to a buffer
 *
 * Accepts local reference to an exception object and converts stores it
 * as a global reference.
 *
 * @param buffer The destination buffer
 * @param exception The pushed object
 */
void jthrowable_circular_buf_push(T_jthrowableCircularBuf *buffer, jthrowable excepetion);



/*
 * Finds an already stored exception object in a buffer
 *
 * The function uses java.lang.Object.equals(@exception) for this purpose.
 *
 * @param buffer The searched buffer
 * @param exception The wanted exception object
 */
jthrowable jthrowable_circular_buf_find(T_jthrowableCircularBuf *buffer, jthrowable excepetion);



#endif // __JTHROWABLE_STASH__



/*
 * finito
 */
