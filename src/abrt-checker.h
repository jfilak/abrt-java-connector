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
#ifndef __ABRT_CHECKER__
#define __ABRT_CHECKER__



/* Macros used to convert __LINE__ into string */
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define __UNUSED_VAR __attribute__ ((unused))


#include <pthread.h>

/*
 * Shared mutex for synchronization of writing.
 */
pthread_mutex_t abrt_print_mutex __UNUSED_VAR;


#ifdef VERBOSE
# define VERBOSE_PRINT(...) \
    do { \
        pthread_mutex_lock(&abrt_print_mutex); \
        fprintf(stdout, __VA_ARGS__); \
        pthread_mutex_unlock(&abrt_print_mutex); \
    } while(0)
#else // !VERBOSE
# define VERBOSE_PRINT(...) do { } while (0)
#endif // VERBOSE

#ifdef SILENT
# define INFO_PRINT(...) do { } while (0)
#else // !SILENT
# define INFO_PRINT(...) \
    do { \
        pthread_mutex_lock(&abrt_print_mutex); \
        fprintf(stdout, __VA_ARGS__); \
        pthread_mutex_unlock(&abrt_print_mutex); \
    } while(0)
#endif // SILENT

#endif // __ABRT_CHECKER__



/*
 * finito
 */
