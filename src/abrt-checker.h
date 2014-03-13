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



/*
 * Flags for specification of destination for error reports
 */
typedef enum {
    ED_TERMINAL = 1,                ///< Report errors to the terminal
    ED_ABRT     = ED_TERMINAL << 1, ///< Submit error reports to ABRT
    ED_SYSLOG   = ED_ABRT << 1,     ///< Submit error reports to syslog
    ED_JOURNALD = ED_SYSLOG << 1,   ///< Submit error reports to journald
} T_errorDestination;



/*
 * Determines which resource is used as executable
 */
enum {
    ABRT_EXECUTABLE_MAIN = 0,
    ABRT_EXECUTABLE_THREAD = 1,
};



/* A pointer determining that log output is disabled */
#define DISABLED_LOG_OUTPUT ((void *)-1)



typedef struct {
    /* Global configuration of report destination */
    T_errorDestination reportErrosTo;

    /* Which frame use for the executable field */
    int executableFlags;

    /* Path (not necessary absolute) to output file */
    char *outputFileName;

    /* Path (not necessary absolute) to configuration file */
    char *configurationFileName;

    /* NULL terminated list of exception types to report when caught */
    char **reportedCaughExceptionTypes;

    /* NULL terminated list of debug methods called when an exceptions is to be
     * reported */
    char **fqdnDebugMethods;

    int configured;
} T_configuration;



/*
 * Initializes an configuration structure
 */
void configuration_initialize(T_configuration *conf);



/*
 * Releases all resources
 */
void configuration_destroy(T_configuration *conf);



/*
 * Parses an options string in form of JVM agent options
 */
void parse_commandline_options(T_configuration *conf, char *options);



/*
 * Parses a configuration file written in libreport configuration file format
 */
void parse_configuration_file(T_configuration *conf, const char *filename);



#endif // __ABRT_CHECKER__



/*
 * finito
 */
