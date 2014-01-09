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
#include "jthrowable_circular_buf.h"
#include "abrt-checker.h"

#include <stdlib.h>
#include <assert.h>



struct jthrowable_circular_buf {
    JNIEnv *jni_env;   ///< required for global reference handling
    size_t capacity;   ///< capacity of the buffer
    size_t begin;      ///< points to the oldest stored object
    size_t end;        ///< points to the newest stored object
    jthrowable *mem;   ///< buffer memory
};



T_jthrowableCircularBuf *jthrowable_circular_buf_new(JNIEnv *jni_env, size_t capacity)
{
    /* I'd throw an exception, but we had to implement this tool in C */
    assert(0 != jni_env || !"Cannot use NULL for a pointer to JNIEnv");
    assert(0 != capacity || !"Cannot use 0 capacity in jthrowable buffer");

    T_jthrowableCircularBuf *buffer = (T_jthrowableCircularBuf *)malloc(sizeof(*buffer));
    if (NULL == buffer)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": malloc() error\n");
        return NULL;
    }

    jthrowable *mem = (jthrowable *)calloc(capacity, sizeof(*mem));
    if (NULL == mem)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": calloc() error\n");
        return NULL;
    }

    buffer->jni_env = jni_env;
    buffer->capacity = capacity;
    buffer->begin = 0;
    buffer->end = 0;
    buffer->mem = mem;

    return buffer;
}



static inline size_t jthrowable_circular_buf_get_index(T_jthrowableCircularBuf *buffer, size_t pos)
{
    assert(NULL != buffer || !"Cannot get a position for a NULL buffer");

    if (pos == buffer->capacity)
    {
        return 0;
    }

    if (pos == (size_t)-1)
    {
        return buffer->capacity - 1;
    }

    return pos;
}



static int jthrowable_circular_buf_empty(T_jthrowableCircularBuf *buffer)
{
    assert(NULL != buffer || !"Cannot use NULL buffer");

    return NULL == buffer->mem[buffer->begin];
}



static void jthrowable_circular_buf_clear(T_jthrowableCircularBuf *buffer)
{
    assert(NULL != buffer || !"Cannot clear NULL buffer");

    VERBOSE_PRINT("Clearing!\n");
    for (size_t i = 0; i < buffer->capacity; ++i)
    {
        if (NULL != buffer->mem[i])
        {
            VERBOSE_PRINT("Cleared %p\n", (void *)buffer->mem[i]);
            (*buffer->jni_env)->DeleteGlobalRef(buffer->jni_env, buffer->mem[i]);
            buffer->mem[i] = NULL;
        }
    }

    buffer->begin = 0;
    buffer->end = 0;
}



void jthrowable_circular_buf_free(T_jthrowableCircularBuf *buffer)
{
    if (NULL == buffer)
    {
        return;
    }

    jthrowable_circular_buf_clear(buffer);

    free(buffer->mem);
    free(buffer);
}



void jthrowable_circular_buf_push(T_jthrowableCircularBuf *buffer, jthrowable exception)
{
    assert(0 != buffer || !"Cannot push an exception object to NULL buffer");
    assert(0 != exception || !"Cannot push a NULL exception to a buffer");

    size_t new_end = buffer->end;

    if (0 == jthrowable_circular_buf_empty(buffer))
    {
        new_end = jthrowable_circular_buf_get_index(buffer, buffer->end + 1);

        if (new_end == buffer->begin)
        {
            (*buffer->jni_env)->DeleteGlobalRef(buffer->jni_env, buffer->mem[buffer->begin]);
            VERBOSE_PRINT("Replacing %p\n", (void *)buffer->mem[buffer->begin]);
            buffer->begin = jthrowable_circular_buf_get_index(buffer, buffer->begin + 1);
        }
    }

    buffer->mem[new_end] = (*buffer->jni_env)->NewGlobalRef(buffer->jni_env, exception);
    VERBOSE_PRINT("Pushed %p\n", (void *)buffer->mem[new_end]);
    buffer->end = new_end;
}



static int jthrowable_circular_buf_find_index(T_jthrowableCircularBuf *buffer, jthrowable exception, size_t *index)
{
    if (0 != jthrowable_circular_buf_empty(buffer))
    {
        return 1;
    }

    jclass object_class = (*buffer->jni_env)->FindClass(buffer->jni_env, "java/lang/Object");
    if ((*buffer->jni_env)->ExceptionOccurred(buffer->jni_env))
    {
        VERBOSE_PRINT("Cannot find java/lang/Object class\n");
#ifdef VERBOSE
        (*buffer->jni_env)->ExceptionDescribe(buffer->jni_env);
#endif
        (*buffer->jni_env)->ExceptionClear(buffer->jni_env);
        return 1;
    }

    if (NULL == object_class)
    {
        VERBOSE_PRINT("Cannot find java/lang/Object class");
        return 1;
    }

    jmethodID equal_method = (*buffer->jni_env)->GetMethodID(buffer->jni_env, object_class, "equals", "(Ljava/lang/Object;)Z");
    if ((*buffer->jni_env)->ExceptionOccurred(buffer->jni_env))
    {
        VERBOSE_PRINT("Cannot find java.lang.Object.equals(Ljava/lang/Object;)Z method\n");
#ifdef VERBOSE
        (*buffer->jni_env)->ExceptionDescribe(buffer->jni_env);
#endif
        (*buffer->jni_env)->ExceptionClear(buffer->jni_env);
        return 1;
    }

    if (NULL == equal_method)
    {
        VERBOSE_PRINT("Cannot find java.lang.Object.equals(Ljava/lang/Object;)Z method");
        (*buffer->jni_env)->DeleteLocalRef(buffer->jni_env, object_class);
        return 1;
    }

    const size_t rbegin = buffer->end;
    const size_t rend = buffer->begin;

    for (size_t i = rbegin; /* break inside */; i = jthrowable_circular_buf_get_index(buffer, (i - 1)))
    {
        VERBOSE_PRINT("Checking next exception object %p\n", (void *)buffer->mem[i]);
        if (NULL != buffer->mem[i])
        {
            jboolean equals = (*buffer->jni_env)->CallBooleanMethod(buffer->jni_env, buffer->mem[i], equal_method, exception);
            if ((*buffer->jni_env)->ExceptionOccurred(buffer->jni_env))
            {
                VERBOSE_PRINT("Cannot determine whether objects are equal\n");
#ifdef VERBOSE
                (*buffer->jni_env)->ExceptionDescribe(buffer->jni_env);
#endif
                (*buffer->jni_env)->ExceptionClear(buffer->jni_env);
                return 1;
            }

            if (equals)
            {
                *index = i;
                return 0;
            }
        }

        if (rend == i)
        {
            break;
        }
    }

    return 1;

}



jthrowable jthrowable_circular_buf_find(T_jthrowableCircularBuf *buffer, jthrowable exception)
{
    assert(0 != buffer || !"Cannot find an exception object in NULL buffer");
    assert(0 != exception || !"Cannot find a NULL exception in a buffer");

    size_t pos = 0;
    if (0 != jthrowable_circular_buf_find_index(buffer, exception, &pos))
    {
        return NULL;
    }

    return buffer->mem[pos];
}



/*
 * finito
 */
