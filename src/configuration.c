#include "abrt-checker.h"

/* ABRT include file */
#include "internal_libabrt.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>



enum {
    OPT_abrt         = 1 << 0,
    OPT_syslog       = 1 << 1,
    OPT_journald     = 1 << 2,
    OPT_output       = 1 << 3,
    OPT_caught       = 1 << 4,
    OPT_executable   = 1 << 5,
    OPT_conffile     = 1 << 6,
    OPT_debugmethod  = 1 << 7,
};



/*
 * Used by load_abrt_plugin_conf_file()
 */
static const char *const s_defaultConfFile = "java.conf";



typedef struct {
    int primarySource;
    const char *listDelimiter;
} T_context;



void configuration_initialize(T_configuration *conf)
{
    memset(conf, 0, sizeof(*conf));
    conf->reportErrosTo = ED_JOURNALD;
    conf->outputFileName = DISABLED_LOG_OUTPUT;
    conf->configurationFileName = (char *)s_defaultConfFile;
}



void configuration_destroy(T_configuration *conf)
{
    if (conf->outputFileName != DISABLED_LOG_OUTPUT)
    {
        free(conf->outputFileName);
    }

    if (conf->configurationFileName != s_defaultConfFile)
    {
        free(conf->configurationFileName);
    }

    free(conf->reportedCaughExceptionTypes);
    free(conf->fqdnDebugMethods);
}



static int skip_separator(const char **input, const char *separator)
{
    size_t i = 0;
    while ((*input)[i] == separator[i])
    {
        if (separator[i] == '\0')
        {
            goto separator_equals;
        }

        ++i;
    }

    if (separator[i] == '\0')
    {
separator_equals:
        (*input) += i;
        return 1;
    }

    ++(*input);
    return 0;
}


/*
 * Returns NULL-terminated char *vector[]. Result itself must be freed,
 * but do no free list elements. IOW: do free(result), but never free(result[i])!
 * If separated_list is NULL or "", returns NULL.
 */
static char **build_string_vector(const char *separated_list, const char *separator)
{
    char **vector = NULL;
    if (separated_list && separated_list[0])
    {
        /* even w/o commas, we'll need two elements:
         * vector[0] = "name"
         * vector[1] = NULL
         */
        unsigned cnt = 2;

        const char *cp = separated_list;
        while (*cp)
            cnt += skip_separator(&cp, separator);

        /* We place the string directly after the char *vector[cnt]: */
        const size_t vector_num_bytes = cnt * sizeof(vector[0]) + (cp - separated_list) + 1;
        vector = malloc(vector_num_bytes);
        if (vector == NULL)
        {
            fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": malloc(): out of memory");
            return NULL;
        }
        vector[cnt-1] = NULL;

        /* Copy the origin string right behind the pointer region */
        char *p = strcpy((char*)&vector[cnt], separated_list);

        char **pp = vector;
        *pp++ = p;
        const size_t sep_len = strlen(separator);
        while (*p)
        {
            if (skip_separator((const char **)&p, separator))
            {
                /* Replace 'separator' by '\0' */
                p[-sep_len] = '\0';
                /* Save pointer to the beginning of next string in the pointer region */
                *pp++ = p;
            }
        }
    }

    return vector;
}



static int parse_option_abrt(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (value != NULL && (strcasecmp("on", value) == 0 || strcasecmp("yes", value) == 0))
    {
        VERBOSE_PRINT("Enabling errors reporting to ABRT\n");
        conf->reportErrosTo |= ED_ABRT;
    }

    return 0;
}



static int parse_option_syslog(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (value != NULL && (strcasecmp("on", value) == 0 || strcasecmp("yes", value) == 0))
    {
        VERBOSE_PRINT("Enabling errors reporting to syslog\n");
        conf->reportErrosTo |= ED_SYSLOG;
    }

    return 0;
}



static int parse_option_journald(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (value != NULL && (strcasecmp("off", value) == 0 || strcasecmp("no", value) == 0))
    {
        VERBOSE_PRINT("Disable errors reporting to JournalD\n");
        conf->reportErrosTo &= ~ED_JOURNALD;
    }

    return 0;
}



static int parse_option_output(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (DISABLED_LOG_OUTPUT != conf->outputFileName)
    {
        free(conf->outputFileName);
    }

    if (value == NULL || value[0] == '\0')
    {
        VERBOSE_PRINT("Disabling output to log file\n");
        conf->outputFileName = DISABLED_LOG_OUTPUT;
    }
    else
    {
        conf->outputFileName = strdup(value);
        if (conf->outputFileName == NULL)
        {
            fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": strdup(output): out of memory\n");
            VERBOSE_PRINT("Can not configure output file to desired value\n");
            /* keep NULL in outputFileName -> the default name will be used */
            return 1;
        }
    }

    return 0;
}



static int parse_option_caught(T_configuration *conf, const char *value, T_context *context)
{
    if (NULL != conf->reportedCaughExceptionTypes)
    {
        free(conf->reportedCaughExceptionTypes);
    }

    conf->reportedCaughExceptionTypes = build_string_vector(value, context->listDelimiter);

    return 0;
}



static int parse_option_executable(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (NULL == value || '\0' == value[0])
    {
        fprintf(stderr, "Value cannot be empty\n");
        return 1;
    }
    else if (strcmp("threadclass", value) == 0)
    {
        VERBOSE_PRINT("Use a thread class for 'executable'\n");
        conf->executableFlags |= ABRT_EXECUTABLE_THREAD;
    }
    else if (strcmp("mainclass", value) == 0)
    {
        /* Unset ABRT_EXECUTABLE_THREAD bit */
        VERBOSE_PRINT("Use the main class for 'executable'\n");
        conf->executableFlags &= ~ABRT_EXECUTABLE_THREAD;
    }
    else
    {
        fprintf(stderr, "Unknown value '%s'\n", value);
        return 1;
    }

    return 0;
}



static int parse_option_conffile(T_configuration *conf, const char *value, T_context *context __UNUSED_VAR)
{
    if (conf->configurationFileName != s_defaultConfFile)
    {
        free(conf->configurationFileName);
    }

    if (NULL == value || '\0' == value[0])
    {
        /* Value cannot be empty */
        VERBOSE_PRINT("Disabling configuration file\n");
        conf->configurationFileName = NULL;
        return 0;
    }

    conf->configurationFileName = strdup(value);
    if (conf->configurationFileName == NULL)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": strdup(output): out of memory\n");
        VERBOSE_PRINT("Can not configure output file to desired value\n");
        /* keep NULL in outputFileName -> the default name will be used */
        return 1;
    }

    return 0;
}



static int parse_option_debugmethod(T_configuration *conf, const char *value, T_context *context)
{
    if (NULL != conf->fqdnDebugMethods)
    {
        free(conf->fqdnDebugMethods);
    }

    conf->fqdnDebugMethods = build_string_vector(value, context->listDelimiter);

    return 0;
}



static void parse_key_value(T_configuration *conf, const char *key, const char *value, T_context *context)
{
    static struct parse_pair {
        int flag;
        const char *key;
        int (*parser)(T_configuration *, const char *, T_context *);
    } arguments[] = {
        { OPT_abrt, "abrt", parse_option_abrt },
        { OPT_syslog, "syslog", parse_option_syslog },
        { OPT_journald, "journald", parse_option_journald },
        { OPT_output, "output", parse_option_output },
        { OPT_caught, "caught", parse_option_caught },
        { OPT_executable, "executable", parse_option_executable },
        { OPT_conffile, "conffile", parse_option_conffile },
        { OPT_debugmethod, "debugmethod", parse_option_debugmethod },
    };

    for (size_t i = 0; i < sizeof(arguments)/sizeof(arguments[0]); ++i)
    {
        if (strcmp(key, arguments[i].key) == 0)
        {
            if ((conf->configured & arguments[i].flag) && !context->primarySource)
            {
                return;
            }

            conf->configured |= arguments[i].flag;

            if (arguments[i].parser(conf, value, context))
            {
                fprintf(stderr, "Error while parsing option '%s'\n", key);
            }
            return;
        }
    }

    fprintf(stderr, "Unknown option '%s'\n", key);
}



/*
 * Parses options passed from the command line and save results in global variables.
 * The function expects string in the following format:
 *  [key[=value][,key[=value]]...]
 *
 *  - separator is ','
 *  - keys without values are allowed
 *  - empty keys are allowed
 *  - multiple occurrences of a single key are allowed
 *  - empty values are allowed
 */
void parse_commandline_options(T_configuration *conf, char *options)
{
    if (NULL == options)
    {
        return;
    }

    T_context ctx = {
        .primarySource = 1,
        .listDelimiter = ":",
    };

    char *savedptr_key = NULL;
    for (char *key = options; /*break inside*/; options=NULL)
    {
        key = strtok_r(options, ",", &savedptr_key);
        if (key == NULL)
        {
            break;
        }

        char *value = strchr(key, '=');
        if (value != NULL)
        {
            value[0] = '\0';
            value += 1;
        }

        VERBOSE_PRINT("Parsed option '%s' = '%s'\n", key, (value ? value : "(None)"));
        parse_key_value(conf, key, value, &ctx);
    }
}



void parse_configuration_file(T_configuration *conf, const char *filename)
{
    /* Remains empty if any of the loading functions below fails */
    map_string_t *settings = new_map_string();
    if (filename[0] == '/')
    {
        load_conf_file(filename, settings, /*skip empty*/0);
    }
    else
    {
        load_abrt_plugin_conf_file(filename, settings);
    }

    T_context ctx = {
        .primarySource = 0, /* do not overwrite already loaded options */
        .listDelimiter = ", ",
    };

    map_string_iter_t iter;
    init_map_string_iter(&iter, settings);
    const char *key;
    const char *value;
    while(next_map_string_iter(&iter, &key, &value))
    {
        parse_key_value(conf, key, value, &ctx);
    }

    free_map_string(settings);
}
