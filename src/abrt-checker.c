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

/*
 * Simple JVMTI Agent demo.
 *
 * Pavel Tisnovsky <ptisnovs@redhat.com>
 * Jakub Filak     <jfilak@redhat.com>
 *
 * It needs to be compiled as a shared native library (.so)
 * and then loaded into JVM using -agentlib command line parameter.
 *
 * Please look into Makefile how the compilation & loading
 * should be performed.
 */

/* System include files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>

/* Shared macros and so on */
#include "abrt-checker.h"

/* ABRT include file */
#include "internal_libabrt.h"

/* JVM TI include files */
#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

/* Internal tool includes */
#include "jthread_map.h"
#include "jthrowable_circular_buf.h"


/* Basic settings */
#define VM_MEMORY_ALLOCATION_THRESHOLD 1024
#define GC_TIME_THRESHOLD 1

/* For debugging purposes */
#define PRINT_JVM_ENVIRONMENT_VARIABLES 1

/* Don't need to be changed */
#define MAX_THREAD_NAME_LENGTH 40

/* Max. length of stack trace */
#define MAX_STACK_TRACE_STRING_LENGTH 10000

/* Depth of stack trace */
#define MAX_STACK_TRACE_DEPTH 5

#define DEFAULT_THREAD_NAME "DefaultThread"

/* Fields which needs to be filled when calling ABRT */
#define FILENAME_TYPE_VALUE      "Java"
#define FILENAME_ANALYZER_VALUE  "Java"

/* Name of two methods from URL class */
#define TO_EXTERNAL_FORM_METHOD_NAME "toExternalForm"
#define GET_PATH_METHOD_NAME "getPath"

/* Default main class name */
#define UNKNOWN_CLASS_NAME "*unknown*"

/* A pointer determining that log output is disabled */
#define DISABLED_LOG_OUTPUT ((void *)-1)

/* The standard stack trace caused by header */
#define CAUSED_STACK_TRACE_HEADER "Caused by: "

/* A number stored reported exceptions */
#ifndef REPORTED_EXCEPTION_STACK_CAPACITY
#define  REPORTED_EXCEPTION_STACK_CAPACITY 5
#endif


/*
 * This structure contains all useful information about JVM environment.
 * (note that these strings should be deallocated using jvmti_env->Deallocate()!)
 */
typedef struct {
    char * cwd;
    char * command_and_params;
    char * launcher;
    char * java_home;
    char * class_path;
    char * boot_class_path;
    char * library_path;
    char * boot_library_path;
    char * ext_dirs;
    char * endorsed_dirs;
    char * java_vm_version;
    char * java_vm_name;
    char * java_vm_info;
    char * java_vm_vendor;
    char * java_vm_specification_name;
    char * java_vm_specification_vendor;
    char * java_vm_specification_version;
} T_jvmEnvironment;



/*
 * This structure contains all usefull information about process
 * where Java virtual machine is started.
 */
typedef struct {
    int    pid;
    char * exec_command;
    char * executable;
    char * main_class;
} T_processProperties;



/*
 * Flags for specification of destination for error reports
 */
typedef enum {
    ED_TERMINAL = 1,                ///< Report errors to the terminal
    ED_ABRT     = ED_TERMINAL << 1, ///< Submit error reports to ABRT
} T_errorDestination;


/* Global monitor lock */
jrawMonitorID lock;

/* Log file */
FILE * fout = NULL;

/* Variable used to measure GC delays */
clock_t gc_start_time;

/* Structure containing JVM environment variables. */
T_jvmEnvironment jvmEnvironment;

/* Structure containing process properties. */
T_processProperties processProperties;

/* Global configuration of report destination */
T_errorDestination reportErrosTo;

/* Path (not necessary absolute) to output file */
char *outputFileName;

/* Path (not necessary absolute) to output file */
char **reportedCaughExceptionTypes;

/* Map of buffer for already reported exceptions to prevent re-reporting */
T_jthreadMap *threadMap;

/* Define a helper macro*/
# define log_print(...) do { if(outputFileName != DISABLED_LOG_OUTPUT) fprintf(fout, __VA_ARGS__); } while(0)



/* forward headers */
static char* get_path_to_class(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass class, char *class_name, const char *stringize_method_name);
static void print_jvm_environment_variables_to_file(FILE *out);
static char* format_class_name(char *class_signature, char replace_to);



/*
 * Returns a static memory with default log file name. Must not be released by free()!
 */
static const char *get_default_log_file_name()
{
    static const char DEFAULT_LOG_FILE_NAME_FORMAT[] = "abrt_checker_%d.log";
    /* A bit more than necessary but this is an optimization and few more Bytes can't kill us */
#define _AUX_LOG_FILE_NAME_MAX_LENGTH (sizeof(DEFAULT_LOG_FILE_NAME_FORMAT) + sizeof(int) * 3)
    static char log_file_name[_AUX_LOG_FILE_NAME_MAX_LENGTH];
    static int initialized = 0;

    if (initialized == 0)
    {
        initialized = 1;

        pid_t pid = getpid();
        /* snprintf() returns -1 on error */
        if (0 > snprintf(log_file_name, _AUX_LOG_FILE_NAME_MAX_LENGTH, DEFAULT_LOG_FILE_NAME_FORMAT, pid))
        {
            fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": snprintf(): can't print default log file name\n");
            return NULL;
        }
    }
#undef _AUX_LOG_FILE_NAME_MAX_LENGTH
    return log_file_name;
}



/*
 * Return PID (process ID) as a string.
 */
static void get_pid_as_string(char * buffer)
{
    int pid = getpid();
    sprintf(buffer, "%d", pid);
    INFO_PRINT("%s\n", buffer);
}



/*
 * Return UID (user ID) as a string.
 */
static void get_uid_as_string(char * buffer)
{
    int uid = getuid();
    sprintf(buffer, "%d", uid);
    INFO_PRINT("%s\n", buffer);
}



/*
 * Return original string or "" if NULL
 * is passed in instead of string.
 */
static const char * null2empty(const char *str)
{
    if (str == NULL)
    {
        return "";
    }
    else
    {
        return str;
    }
}



/*
 * Returns non zero value if exception's type is intended to be reported even
 * if the exception was caught.
 */
static int exception_is_intended_to_be_reported(const char *type_name)
{
    if (reportedCaughExceptionTypes != NULL)
    {
        /* special cases for selected exceptions */
        for (char **cursor = reportedCaughExceptionTypes; *cursor; ++cursor)
        {
            if (strcmp(*cursor, type_name) == 0)
            {
                return 1;
            }
        }
    }

    return 0;
}



/*
 * Add JVM environment data into ABRT event message.
 */
static void add_jvm_environment_data(problem_data_t *pd)
{
    char *jvm_env = NULL;
    size_t sizeloc = 0;
    FILE *mem = open_memstream(&jvm_env, &sizeloc);

    if (NULL == mem)
    {
        perror("Skipping 'jvm_environment' problem element. open_memstream");
        return;
    }

    print_jvm_environment_variables_to_file(mem);
    fclose(mem);

    problem_data_add_text_editable(pd, "jvm_environment", jvm_env);
    free(jvm_env);
}



/*
 * Add process properties into ABRT event message.
 */
static void add_process_properties_data(problem_data_t *pd)
{
    pid_t pid = getpid();

    char *environ = get_environ(pid);
    problem_data_add_text_editable(pd, FILENAME_ENVIRON, environ ? environ : "");
    free(environ);

    char pidstr[20];
    get_pid_as_string(pidstr);
    problem_data_add_text_editable(pd, FILENAME_PID, pidstr);
    problem_data_add_text_editable(pd, FILENAME_CMDLINE, null2empty(processProperties.exec_command));
    if (!problem_data_get_content_or_NULL(pd, FILENAME_EXECUTABLE))
    {
        problem_data_add_text_editable(pd, FILENAME_EXECUTABLE, null2empty(processProperties.executable));
    }
    else
    {
        problem_data_add_text_editable(pd, "java_executable", null2empty(processProperties.executable));
    }
}



/*
 * Register new ABRT event using given message and a method name.
 * If reportErrosTo global flags doesn't contain ED_ABRT, this function does nothing.
 */
static void register_abrt_event(char * executable, char * message, unsigned char * method, char * backtrace)
{
    if ((reportErrosTo & ED_ABRT) == 0)
    {
        VERBOSE_PRINT("ABRT reporting is disabled\n");
        return;
    }

    char abrt_message[1000];
    char s[11];
    problem_data_t *pd = problem_data_new();

    /* fill in all required fields */
    problem_data_add_text_editable(pd, FILENAME_TYPE, FILENAME_TYPE_VALUE);
    problem_data_add_text_editable(pd, FILENAME_ANALYZER, FILENAME_ANALYZER_VALUE);

    get_uid_as_string(s);
    problem_data_add_text_editable(pd, FILENAME_UID, s);

    sprintf(abrt_message, "%s in method %s", message, method);

    /* executable must belong to some package otherwise ABRT refuse it */
    problem_data_add_text_editable(pd, FILENAME_EXECUTABLE, executable);
    problem_data_add_text_editable(pd, FILENAME_BACKTRACE, backtrace);

    /* type and analyzer are the same for abrt, we keep both just for sake of comaptibility */
    problem_data_add_text_editable(pd, FILENAME_REASON, abrt_message);
    /* end of required fields */

    /* add optional fields */
    add_jvm_environment_data(pd);
    add_process_properties_data(pd);

    /* sends problem data to abrtd over the socket */
    int res = problem_data_send_to_abrt(pd);
    fprintf(stderr, "ABRT problem creation: '%s'\n", res ? "failure" : "success");
    problem_data_free(pd);
}



/*
 * Print a message when any JVM TI error occurs.
 */
static void print_jvmti_error(
            jvmtiEnv   *jvmti_env,
            jvmtiError  error_code,
            const char *str)
{
    char *errnum_str;
    const char *msg_str = str == NULL ? "" : str;
    char *msg_err = NULL;
    errnum_str = NULL;

    /* try to convert error number to string */
    (void)(*jvmti_env)->GetErrorName(jvmti_env, error_code, &errnum_str);
    msg_err = errnum_str == NULL ? "Unknown" : errnum_str;
    fprintf(stderr, "ERROR: JVMTI: %d(%s): %s\n", error_code, msg_err, msg_str);
}


static int get_tid(
        JNIEnv   *jni_env,
        jthread  thr,
        jlong    *tid)
{
    jclass thread_class = (*jni_env)->GetObjectClass(jni_env, thr);
    if (NULL == thread_class)
    {
        VERBOSE_PRINT("Cannot get class of thread object\n");
        return 1;
    }

    jmethodID get_id = (*jni_env)->GetMethodID(jni_env, thread_class, "getId", "()J" );
    if (NULL == get_id)
    {
        VERBOSE_PRINT("Cannot method java.lang.Thread.getId()J\n");
        return 1;
    }

    *tid = (*jni_env)->CallLongMethod(jni_env, thr, get_id);

    return 0;
}



/*
 * Format class signature into a printable form.
 * Class names have form "Ljava/lang/String;"
 * Requested form        "java.lang.String"
 */
static char* format_class_name(char *class_signature, char replace_to)
{
    char *output;
    /* make sure we don't end with NPE */
    if (class_signature != NULL)
    {
        /* replace 'L' from the beggining of class signature */
        output = class_signature;
        if (output[0] == 'L')
        {
            output++; /* goto to the next character */
        }
        /* replace the last character in the class name */
        /* but inly if this character is ';' */
        char *last_char = output + strlen(output) - 1;
        if (*last_char == ';')
        {
            *last_char = replace_to;
        }
        /* replace all '/'s to '.'s */
        char *c;
        for (c = class_signature; *c != 0; c++)
        {
            if (*c == '/') *c = '.';
        }
    }
    else
    {
        return NULL;
    }
    return output;
}



/*
 * Format class signature into a form suitable for ClassLoader.getResource()
 * Class names have form "Ljava/lang/String;"
 * Requested form        "java/lang/String"
 */
char* format_class_name_for_JNI_call(char *class_signature)
{
    char *output;
    /* make sure we don't end with NPE */
    if (class_signature != NULL)
    {
        /* replace 'L' from the beggining of class signature */
        output = class_signature;
        if (output[0] == 'L')
        {
            output++; /* goto to the next character */
        }
        /* replace last character in the class name */
        /* if this character is ';' */
        char *last_char = output + strlen(output) - 1;
        if (*last_char == ';')
        {
            *last_char = '.';
        }
    }
    else
    {
        return NULL;
    }
    return output;
}



/*
 * Check if any JVM TI error have occured.
 */
static int check_jvmti_error(
            jvmtiEnv   *jvmti_env,
            jvmtiError  error_code,
            const char *str)
{
    if ( error_code != JVMTI_ERROR_NONE )
    {
        print_jvmti_error(jvmti_env, error_code, str);
        return 1;
    }

    return 0;
}



/*
 * Enter a critical section by doing a JVMTI Raw Monitor Enter
 */
static void enter_critical_section(
            jvmtiEnv *jvmti_env)
{
    jvmtiError error_code;

    error_code = (*jvmti_env)->RawMonitorEnter(jvmti_env, lock);
    check_jvmti_error(jvmti_env, error_code, "Cannot enter with raw monitor");
}



/*
 * Exit a critical section by doing a JVMTI Raw Monitor Exit
 */
static void exit_critical_section(
            jvmtiEnv *jvmti_env)
{
    jvmtiError error_code;

    error_code = (*jvmti_env)->RawMonitorExit(jvmti_env, lock);
    check_jvmti_error(jvmti_env, error_code, "Cannot exit with raw monitor");
}



/*
 * Get a name for a given jthread.
 */
static void get_thread_name(
            jvmtiEnv *jvmti_env,
            jthread   thread,
            char     *tname,
            int       maxlen)
{
    jvmtiThreadInfo info;
    jvmtiError      error;

    /* Make sure the stack variables are garbage free */
    (void)memset(&info, 0, sizeof(info));

    /* Assume the name is unknown for now */
    (void)strcpy(tname, DEFAULT_THREAD_NAME);

    /* Get the thread information, which includes the name */
    error = (*jvmti_env)->GetThreadInfo(jvmti_env, thread, &info);
    check_jvmti_error(jvmti_env, error, "Cannot get thread info");

    /* The thread might not have a name, be careful here. */
    if (info.name != NULL)
    {
        int len;

        /* Copy the thread name into tname if it will fit */
        len = (int)strlen(info.name);
        if ( len < maxlen )
        {
            (void)strcpy(tname, info.name);
        }

        error = ((*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)info.name));
        if (error != JVMTI_ERROR_NONE)
        {
            INFO_PRINT("(get_thread_name) Error expected: %d, got: %d\n", JVMTI_ERROR_NONE, error);
            INFO_PRINT("\n");
        }
    }
}



/*
 * Read executable name from link /proc/${PID}/exe
 */
static char* malloc_readlink(const char *linkname)
{
    char buf[PATH_MAX + 1];
    int len;

    len = readlink(linkname, buf, sizeof(buf)-1);
    if (len >= 0)
    {
        buf[len] = '\0';
        char *p = malloc(strlen(buf) + 1);
        if (p)
        {
            strcpy(p, buf);
        }
        return p;
    }
    return NULL;
}



/*
 * Read executable name from the special file /proc/${PID}/exe
 */
char *get_executable(int pid)
{
    /* be sure we allocated enough memory for path to a file /proc/${PID}/exe */
    char buf[sizeof("/proc/%lu/exe") + sizeof(long)*3];

    sprintf(buf, "/proc/%lu/exe", (long)pid);
    char *executable = malloc_readlink(buf);
    if (!executable)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": can't read executable name from /proc/${PID}/exe");
        return NULL;
    }

    /* find and cut off " (deleted)" from the path */
    char *deleted = executable + strlen(executable) - strlen(" (deleted)");
    if (deleted > executable && strcmp(deleted, " (deleted)") == 0)
    {
        *deleted = '\0';
        /*log("File '%s' seems to be deleted", executable);*/
    }

    /* find and cut off prelink suffixes from the path */
    char *prelink = executable + strlen(executable) - strlen(".#prelink#.XXXXXX");
    if (prelink > executable && strncmp(prelink, ".#prelink#.", strlen(".#prelink#.")) == 0)
    {
        /*log("File '%s' seems to be a prelink temporary file", executable);*/
        *prelink = '\0';
    }
    return executable;
}



/*
 * Read command parameters from /proc/${PID}/cmdline
 */
char *get_command(int pid)
{
    char file_name[32];
    FILE *fin;
    size_t size = 0;
    char *out;
    char buffer[2048];

    /* name of /proc/${PID}/cmdline */
    sprintf(file_name, "/proc/%d/cmdline", pid);

    /* read first 2047 bytes from this file */
    fin = fopen(file_name, "rb");
    if (NULL == fin)
    {
        return NULL;
    }

    size = fread(buffer, sizeof(char), 2048, fin);
    fclose(fin);

    /* parameters are divided by \0, get rid of it */
    for (size_t i=0; i<size-1; i++)
    {
        if (buffer[i] == 0) buffer[i] = ' ';
    }

    /* defensive copy */
    out = (char*)calloc(strlen(buffer)+1, sizeof(char));
    strcpy(out, buffer);
    return out;
}



/*
 * Replace all old_chars by new_chars
 */
static void string_replace(char *string_to_replace, char old_char, char new_char)
{
    char *c = string_to_replace;
    for (; *c; c++)
    {
        if (*c==old_char) *c=new_char;
    }
}



/*
 * Appends '.' and returns the result a newly mallocated memory
 */
static char * create_updated_class_name(char *class_name)
{
    char *upd_class_name = (char*)malloc(strlen(class_name)+2);
    if (NULL == upd_class_name)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": malloc(): out of memory");
        return NULL;
    }
    strcpy(upd_class_name, class_name);
    strcat(upd_class_name, ".");
    return upd_class_name;
}



/*
 * Solution for JAR-style URI:
 * file:/home/tester/abrt_connector/bin/JarTest.jar!/SimpleTest.class
 */
static char * stripped_path_to_main_class(char *path_to_main_class)
{
    /* strip "file:" from the beginning */
    char *out = path_to_main_class + sizeof("file:") - 1;

    char *excl = strchr(out, '!');

    /* strip everything after '!' */
    if (excl != NULL)
    {
        *excl = 0;
    }
    return out;
}



/*
 * Get name and path to main class.
 */
static char *get_main_class(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env)
{
    jvmtiError error_code;
    char *class_name;

    error_code = (*jvmti_env)->GetSystemProperty(jvmti_env, "sun.java.command", &class_name);
    if (error_code != JVMTI_ERROR_NONE)
    {
        return UNKNOWN_CLASS_NAME;
    }

    /* strip the second part of sun.java.command property */
    char *space = strchrnul(class_name, ' ');
    *space = 0;

    /* replace all '.' to '/' */
    string_replace(class_name, '.', '/');

    jclass cls = (*jni_env)->FindClass(jni_env, class_name);
    /* Throws:
     * ClassFormatError: if the class data does not specify a valid class. 
     * ClassCircularityError: if a class or interface would be its own superclass or superinterface. 
     * NoClassDefFoundError: if no definition for a requested class or interface can be found. 
     * OutOfMemoryError: if the system runs out of memory.
     */

    jthrowable exception;
    exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (exception)
    {
        (*jni_env)->ExceptionClear(jni_env);
    }

    if (cls == NULL)
    {
        if (class_name != NULL)
        {
            (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)class_name);
        }
        return UNKNOWN_CLASS_NAME;
    }

    /* add '.' at the end of class name */
    char *upd_class_name = create_updated_class_name(class_name);

    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)class_name);

    if (upd_class_name == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, cls);
        return NULL;
    }

    char *path_to_class = get_path_to_class(jvmti_env, jni_env, cls, upd_class_name, GET_PATH_METHOD_NAME);

    free(upd_class_name);

    (*jni_env)->DeleteLocalRef(jni_env, cls);

    if (path_to_class == NULL)
    {
        return UNKNOWN_CLASS_NAME;
    }

    /* Solution for JAR-style URI:
     * file:/home/tester/abrt_connector/bin/JarTest.jar!/SimpleTest.class
     */
    if (strncpy("file:", path_to_class, sizeof("file:") == 0))
    {
        return stripped_path_to_main_class(path_to_class);
    }

    /* path_to_class is allocated on heap -> ok to return this pointer */
    return path_to_class;
}



/*
 * Fill in the structure processProperties with JVM process info.
 */
static void fill_process_properties(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env)
{
    int pid = getpid();
    processProperties.pid = pid;
    processProperties.executable = get_executable(pid);
    processProperties.exec_command = get_command(pid);
    processProperties.main_class = get_main_class(jvmti_env, jni_env);
}



/*
 * Fill in the structure jvmEnvironment with JVM info.
 */
static void fill_jvm_environment(
            jvmtiEnv *jvmti_env)
{
    (*jvmti_env)->GetSystemProperty(jvmti_env, "sun.java.command", &(jvmEnvironment.command_and_params));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "sun.java.launcher", &(jvmEnvironment.launcher));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.home", &(jvmEnvironment.java_home));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.class.path", &(jvmEnvironment.class_path));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.library.path", &(jvmEnvironment.library_path));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "sun.boot.class.path", &(jvmEnvironment.boot_class_path));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "sun.boot.library.path", &(jvmEnvironment.boot_library_path));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.ext.dirs", &(jvmEnvironment.ext_dirs));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.endorsed.dirs", &(jvmEnvironment.endorsed_dirs));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.version", &(jvmEnvironment.java_vm_version));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.name", &(jvmEnvironment.java_vm_name));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.info", &(jvmEnvironment.java_vm_info));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.vendor", &(jvmEnvironment.java_vm_vendor));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.specification.name", &(jvmEnvironment.java_vm_specification_name));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.specification.vendor", &(jvmEnvironment.java_vm_specification_vendor));
    (*jvmti_env)->GetSystemProperty(jvmti_env, "java.vm.specification.version", &(jvmEnvironment.java_vm_specification_version));

    jvmEnvironment.cwd = getcwd(NULL, 0);
}



/*
 * Print process properties.
 */
static void print_process_properties(void)
{
    INFO_PRINT("%-30s: %d\n", "pid", processProperties.pid);
    INFO_PRINT("%-30s: %s\n", "executable", processProperties.executable);
    INFO_PRINT("%-30s: %s\n", "exec_command", processProperties.exec_command);
    INFO_PRINT("%-30s: %s\n", "main_class", processProperties.main_class);
}



/*
 * Print JVM environment
 */
static void print_jvm_environment_variables(void)
{
#ifndef SILENT
    print_jvm_environment_variables_to_file(stdout);
#endif
}

static void print_jvm_environment_variables_to_file(FILE *out)
{
    fprintf(out, "%-30s: %s\n", "sun.java.command", null2empty(jvmEnvironment.command_and_params));
    fprintf(out, "%-30s: %s\n", "sun.java.launcher", null2empty(jvmEnvironment.launcher));
    fprintf(out, "%-30s: %s\n", "java.home", null2empty(jvmEnvironment.java_home));
    fprintf(out, "%-30s: %s\n", "java.class.path", null2empty(jvmEnvironment.class_path));
    fprintf(out, "%-30s: %s\n", "java.library.path", null2empty(jvmEnvironment.library_path));
    fprintf(out, "%-30s: %s\n", "sun.boot.class.path", null2empty(jvmEnvironment.boot_class_path));
    fprintf(out, "%-30s: %s\n", "sun.boot.library.path", null2empty(jvmEnvironment.boot_library_path));
    fprintf(out, "%-30s: %s\n", "java.ext.dirs", null2empty(jvmEnvironment.ext_dirs));
    fprintf(out, "%-30s: %s\n", "java.endorsed.dirs", null2empty(jvmEnvironment.endorsed_dirs));
    fprintf(out, "%-30s: %s\n", "cwd", null2empty(jvmEnvironment.cwd));
    fprintf(out, "%-30s: %s\n", "java.vm.version", null2empty(jvmEnvironment.java_vm_version));
    fprintf(out, "%-30s: %s\n", "java.vm.name", null2empty(jvmEnvironment.java_vm_name));
    fprintf(out, "%-30s: %s\n", "java.vm.info", null2empty(jvmEnvironment.java_vm_info));
    fprintf(out, "%-30s: %s\n", "java.vm.vendor", null2empty(jvmEnvironment.java_vm_vendor));
    fprintf(out, "%-30s: %s\n", "java.vm.specification_name", null2empty(jvmEnvironment.java_vm_specification_name));
    fprintf(out, "%-30s: %s\n", "java.vm.specification.vendor", null2empty(jvmEnvironment.java_vm_specification_vendor));
    fprintf(out, "%-30s: %s\n", "java.vm.specification.version", null2empty(jvmEnvironment.java_vm_specification_version));
}



/*
 * Called right after JVM started up.
 */
static void JNICALL callback_on_vm_init(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            jthread   thread)
{
    char tname[MAX_THREAD_NAME_LENGTH];

    enter_critical_section(jvmti_env);

    INFO_PRINT("Got VM init event\n");
    get_thread_name(jvmti_env , thread, tname, sizeof(tname));
    INFO_PRINT("callbackVMInit:  %s thread\n", tname);

    fill_jvm_environment(jvmti_env);
    fill_process_properties(jvmti_env, jni_env);
#if PRINT_JVM_ENVIRONMENT_VARIABLES == 1
    print_jvm_environment_variables();
    print_process_properties();
#endif
    exit_critical_section(jvmti_env);
}



/*
 * Called before JVM shuts down.
 */
static void JNICALL callback_on_vm_death(
            jvmtiEnv *jvmti_env,
            JNIEnv   *env __UNUSED_VAR)
{
    enter_critical_section(jvmti_env);
    INFO_PRINT("Got VM Death event\n");
    exit_critical_section(jvmti_env);
}



/*
 * Called before thread end.
 */
static void JNICALL callback_on_thread_start(
            jvmtiEnv *jvmti_env __UNUSED_VAR,
            JNIEnv   *jni_env,
            jthread  thread)
{
    INFO_PRINT("ThreadStart\n");
    if (NULL == threadMap)
    {
        return;
    }

    jlong tid = 0;

    if (get_tid(jni_env, thread, &tid))
    {
        VERBOSE_PRINT("Cannot malloc thread's exception buffer because cannot get TID");
        return;
    }

    T_jthrowableCircularBuf *threads_exc_buf = jthrowable_circular_buf_new(jni_env, REPORTED_EXCEPTION_STACK_CAPACITY);
    if (NULL == threads_exc_buf)
    {
        fprintf(stderr, "Cannot enable check for already reported exceptions. Disabling reporting to ABRT in current thread!");
        return;
    }

    jthread_map_push(threadMap, tid, threads_exc_buf);
}



/*
 * Called before thread end.
 */
static void JNICALL callback_on_thread_end(
            jvmtiEnv *jvmti_env __UNUSED_VAR,
            JNIEnv   *jni_env,
            jthread  thread)
{
    INFO_PRINT("ThreadEnd\n");
    if (NULL == threadMap)
    {
        return;
    }

    jlong tid = 0;

    if (get_tid(jni_env, thread, &tid))
    {
        VERBOSE_PRINT("Cannot free thread's exception buffer because cannot get TID");
        return;
    }

    T_jthrowableCircularBuf *threads_exc_buf = jthread_map_pop(threadMap, tid);
    if (threads_exc_buf != NULL)
    {
        jthrowable_circular_buf_free(threads_exc_buf);
    }
}


#ifdef GENERATE_JVMTI_STACK_TRACE
/*
 * Get line number for given method and location in this method.
 */
static int get_line_number(
            jvmtiEnv  *jvmti_env,
            jmethodID  method,
            jlocation  location)
{
    int count;
    int line_number = 0;
    int i;
    jvmtiLineNumberEntry *location_table;
    jvmtiError error_code;

    /* how can we recognize an illegal value of the location variable.
     * Documentation says:
     *   A 64 bit value, representing a monotonically increasing executable
     *   position within a method. -1 indicates a native method.
     *
     * we use 0 for now.
     */
    if (NULL == method || 0 == location)
    {
        return -1;
    }

    /* read table containing line numbers and instruction indexes */
    error_code = (*jvmti_env)->GetLineNumberTable(jvmti_env, method, &count, &location_table);
    /* it is possible, that we are unable to read the table -> missing debuginfo etc. */
    if (error_code != JVMTI_ERROR_NONE)
    {
        if (location_table != NULL)
        {
            (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)location_table);
        }
        return -1;
    }

    /* iterate over the read table */
    for (i = 0; i < count - 1; i++)
    {
        jvmtiLineNumberEntry entry1 = location_table[i];
        jvmtiLineNumberEntry entry2 = location_table[i+1];
        /* entry1 and entry2 are copies allocated on the stack:          */
        /*   how can we recognize that location_table[i] is valid value? */
        /*                                                               */
        /* we hope that all array values are valid for now               */

        /* if location is between entry1 (including) and entry2 (excluding), */
        /* we are on the right line */
        if (location >= entry1.start_location && location < entry2.start_location)
        {
            line_number = entry1.line_number;
            break;
        }
    }

    /* last instruction is handled specifically */
    if (location >= location_table[count-1].start_location)
    {
        line_number = location_table[count-1].line_number;
    }

    /* memory deallocation */
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)location_table);
    return line_number;
}
#endif /* GENERATE_JVMTI_STACK_TRACE */



/*
 * Return path to given class using given class loader.
 */
static char* get_path_to_class_class_loader(
            jvmtiEnv *jvmti_env __UNUSED_VAR,
            JNIEnv   *jni_env,
            jclass    class_loader,
            char     *class_name,
            const char *stringize_method_name)
{
    jclass class_loader_class = NULL;

    char *upd_class_name = (char*)malloc(strlen(class_name) + sizeof("class") + 1);
    if (NULL == upd_class_name)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": malloc(): out of memory");
        return NULL;
    }

    strcpy(upd_class_name, class_name);
    strcat(upd_class_name, "class");

    /* find ClassLoader class */
    class_loader_class = (*jni_env)->FindClass(jni_env, "java/lang/ClassLoader");
    /* Throws:
     * ClassFormatError: if the class data does not specify a valid class. 
     * ClassCircularityError: if a class or interface would be its own superclass or superinterface. 
     * NoClassDefFoundError: if no definition for a requested class or interface can be found. 
     * OutOfMemoryError: if the system runs out of memory.
     */

    /* check if exception was thrown from FindClass() method */
    jthrowable exception;
    exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (exception)
    {
        (*jni_env)->ExceptionClear(jni_env);
        free(upd_class_name);
        return NULL;
    }

    if (class_loader_class ==  NULL)
    {
        free(upd_class_name);
        return NULL;
    }

    /* find method ClassLoader.getResource() */
    jmethodID get_resource = (*jni_env)->GetMethodID(jni_env, class_loader_class, "getResource", "(Ljava/lang/String;)Ljava/net/URL;" );
    /* Throws:
     * NoSuchMethodError: if the specified method cannot be found. 
     * ExceptionInInitializerError: if the class initializer fails due to an exception. 
     * OutOfMemoryError: if the system runs out of memory.
     */

    exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (exception)
    {
        free(upd_class_name);
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    if (get_resource ==  NULL)
    {
        free(upd_class_name);
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    /* convert new class name into a Java String */
    jstring j_class_name = (*jni_env)->NewStringUTF(jni_env, upd_class_name);
    free(upd_class_name);

    /* call method ClassLoader.getResource(className) */
    jobject url = (*jni_env)->CallObjectMethod(jni_env, class_loader, get_resource, j_class_name);
    if ((*jni_env)->ExceptionCheck(jni_env))
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
        (*jni_env)->ExceptionClear(jni_env);
        return NULL;
    }
    if (url ==  NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
        return NULL;
    }

    /* find method URL.toString() */
    jmethodID to_external_form = (*jni_env)->GetMethodID(jni_env, (*jni_env)->FindClass(jni_env, "java/net/URL"), stringize_method_name, "()Ljava/lang/String;" );
    /* Throws:
     * NoSuchMethodError: if the specified method cannot be found. 
     * ExceptionInInitializerError: if the class initializer fails due to an exception. 
     * OutOfMemoryError: if the system runs out of memory.
     */
    exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (exception)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
        return NULL;
    }

    /* call method URL.toString() */
    jstring jstr = (jstring)(*jni_env)->CallObjectMethod(jni_env, url, to_external_form);
    /* no exception expected */

    if (jstr ==  NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
        return NULL;
    }

    /* convert Java String into C char* */
    char *str = (char*)(*jni_env)->GetStringUTFChars(jni_env, jstr, NULL);
    char *out = strdup(str);
    if (out == NULL)
    {
        fprintf(stderr, "strdup(): out of memory");
    }

    /* cleanup */
    (*jni_env)->ReleaseStringUTFChars(jni_env, jstr, str);
    (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
    (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
    return out;
}



/*
 * Wraps java.lang.ClassLoader.getSystemClassLoader()
 */
static jobject get_system_class_loader(
            jvmtiEnv *jvmti_env __UNUSED_VAR,
            JNIEnv   *jni_env)
{
    jclass class_loader_class = (*jni_env)->FindClass(jni_env, "java/lang/ClassLoader");
    if (NULL == class_loader_class)
    {
        VERBOSE_PRINT("Cannot find java/lang/ClassLoader class\n");
        return NULL;
    }

    jmethodID get_system_class_loader_smethod =(*jni_env)->GetStaticMethodID(jni_env, class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jthrowable exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (NULL != exception)
    {
        VERBOSE_PRINT("Exception occured: can not get method java.lang.ClassLoader.getSystemClassLoader()\n");
        (*jni_env)->ExceptionClear(jni_env);
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }
    if (NULL == get_system_class_loader_smethod)
    {
        VERBOSE_PRINT("Cannot find java.lang.ClassLoader.getSystemClassLoader()Ljava/lang/ClassLoader;\n");
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    jobject system_class_loader = (*jni_env)->CallStaticObjectMethod(jni_env, class_loader_class, get_system_class_loader_smethod);
    exception = (*jni_env)->ExceptionOccurred(jni_env);
    if (NULL != exception)
    {
        VERBOSE_PRINT("java.lang.ClassLoader.getSystemClassLoader() thrown an exception\n");
        (*jni_env)->ExceptionClear(jni_env);
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
    return system_class_loader;
}



/*
 * Return path to given class.
 */
static char* get_path_to_class(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            jclass    class,
            char     *class_name,
            const char *stringize_method_name)
{
    jobject class_loader = NULL;
    (*jvmti_env)->GetClassLoader(jvmti_env, class, &class_loader);

    /* class is loaded using boot classloader */
    if (class_loader == NULL)
    {
        VERBOSE_PRINT("A class has not been loaded by a ClassLoader. Going to use the system class loader.\n");

        class_loader = get_system_class_loader(jvmti_env, jni_env);
        if (NULL == class_loader)
        {
            VERBOSE_PRINT("Cannot get the system class loader.");
            return NULL;
        }
    }

    return get_path_to_class_class_loader(jvmti_env, jni_env, class_loader, class_name, stringize_method_name);
}


static jclass find_class_in_loaded_class(
            jvmtiEnv   *jvmti_env,
            JNIEnv     *jni_env,
            const char *searched_class_name)
{
    jint num_classes = 0;
    jclass *loaded_classes;
    jvmtiError error = (*jvmti_env)->GetLoadedClasses(jvmti_env, &num_classes, &loaded_classes);
    if (check_jvmti_error(jvmti_env, error, "jvmtiEnv::GetLoadedClasses()"))
    {
        return NULL;
    }

    jclass class_class = (*jni_env)->FindClass(jni_env, "java/lang/Class");
    if (NULL == class_class)
    {
        VERBOSE_PRINT("Cannot find java/lang/Class class");
        return NULL;
    }

    jmethodID get_name_method = (*jni_env)->GetMethodID(jni_env, class_class, "getName", "()Ljava/lang/String;");
    if (NULL == get_name_method)
    {
        VERBOSE_PRINT("Cannot find java.lang.Class.getName.()Ljava/lang/String;");
        (*jni_env)->DeleteLocalRef(jni_env, class_class);
        return NULL;
    }

    jclass result = NULL;
    for (jint i = 0; NULL == result && i < num_classes; ++i)
    {
        jobject class_name = (*jni_env)->CallObjectMethod(jni_env, loaded_classes[i], get_name_method);
        if (NULL == class_name)
        {
            continue;
        }

        char *class_name_cstr = (char*)(*jni_env)->GetStringUTFChars(jni_env, class_name, NULL);
        if (strcmp(searched_class_name, class_name_cstr) == 0)
        {
            VERBOSE_PRINT("The class was found in the array of loaded classes\n");
            result = loaded_classes[i];
        }

        (*jni_env)->ReleaseStringUTFChars(jni_env, class_name, class_name_cstr);
        (*jni_env)->DeleteLocalRef(jni_env, class_name);
    }

    /* Not calling DeleteLocalRef() on items in loaded_classes. Hopefully they will be deleted automatically */
    return result;
}



/*
 * Print one method from stack frame.
 */
static int print_stack_trace_element(
            jvmtiEnv       *jvmti_env,
            JNIEnv         *jni_env,
            jobject         stack_frame,
            char           *stack_trace_str,
            unsigned        max_length)
{
    jclass stack_frame_class = (*jni_env)->GetObjectClass(jni_env, stack_frame);
    jmethodID get_class_name_method = (*jni_env)->GetMethodID(jni_env, stack_frame_class, "getClassName", "()Ljava/lang/String;");
    if (get_class_name_method == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, stack_frame_class);
        return -1;
    }

    jstring class_name_of_frame_method = (*jni_env)->CallObjectMethod(jni_env, stack_frame, get_class_name_method);
    if ((*jni_env)->ExceptionOccurred(jni_env))
    {
        (*jni_env)->DeleteLocalRef(jni_env, stack_frame_class);
        (*jni_env)->ExceptionClear(jni_env);
        return -1;
    }
    if (class_name_of_frame_method == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, stack_frame_class);
        return -1;
    }

    char *cls_name_str = (char*)(*jni_env)->GetStringUTFChars(jni_env, class_name_of_frame_method, NULL);
    string_replace(cls_name_str, '.', '/');
    jclass class_of_frame_method = (*jni_env)->FindClass(jni_env, cls_name_str);
    char *class_location = NULL;

    if ((*jni_env)->ExceptionOccurred(jni_env))
    {
        VERBOSE_PRINT(__FILE__ ":" STRINGIZE(__LINE__)": FindClass(%s) thrown an exception\n", cls_name_str);
        (*jni_env)->ExceptionClear(jni_env);

        string_replace(cls_name_str, '/', '.');
        class_of_frame_method = find_class_in_loaded_class(jvmti_env, jni_env, cls_name_str);
        string_replace(cls_name_str, '.', '/');
    }

    if (NULL != class_of_frame_method)
    {
        char *updated_cls_name_str = create_updated_class_name(cls_name_str);
        if (updated_cls_name_str != NULL)
        {
            class_location = get_path_to_class(jvmti_env, jni_env, class_of_frame_method, updated_cls_name_str, TO_EXTERNAL_FORM_METHOD_NAME);
            free(updated_cls_name_str);
        }
        (*jni_env)->DeleteLocalRef(jni_env, class_of_frame_method);
    }
    (*jni_env)->ReleaseStringUTFChars(jni_env, class_name_of_frame_method, cls_name_str);

    jmethodID to_string_method = (*jni_env)->GetMethodID(jni_env, stack_frame_class, "toString", "()Ljava/lang/String;");
    (*jni_env)->DeleteLocalRef(jni_env, stack_frame_class);
    if (to_string_method == NULL)
    {
        return -1;
    }

    jobject orig_str = (*jni_env)->CallObjectMethod(jni_env, stack_frame, to_string_method);
    if ((*jni_env)->ExceptionOccurred(jni_env))
    {
        (*jni_env)->DeleteLocalRef(jni_env, orig_str);
        (*jni_env)->ExceptionClear(jni_env);
        return -1;
    }
    if (orig_str == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, orig_str);
        return -1;
    }

    char *str = (char*)(*jni_env)->GetStringUTFChars(jni_env, orig_str, NULL);
    int wrote = snprintf(stack_trace_str, max_length, "\tat %s [%s]\n", str, class_location == NULL ? "unknown" : class_location);
    if (wrote > 0 && stack_trace_str[wrote-1] != '\n')
    {   /* the length limit was reached and frame is printed only partially */
        /* so in order to not show partial frames clear current frame's data */
        VERBOSE_PRINT("Too many frames or too long frame. Finishing stack trace generation.");
        stack_trace_str[0] = '\0';
        wrote = 0;
    }
    (*jni_env)->ReleaseStringUTFChars(jni_env, orig_str, str);
    (*jni_env)->DeleteLocalRef(jni_env, orig_str);
    return wrote;
}



/*
 * Generates standard Java exception stack trace with file system path to the file
 */
static int print_exception_stack_trace(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            jobject   exception,
            char     *stack_trace_str,
            size_t    max_stack_trace_lenght)
{

    jclass exception_class = (*jni_env)->GetObjectClass(jni_env, exception);
    jmethodID to_string_method = (*jni_env)->GetMethodID(jni_env, exception_class, "toString", "()Ljava/lang/String;");
    if (to_string_method == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, exception_class);
        return -1;
    }

    jobject exception_str = (*jni_env)->CallObjectMethod(jni_env, exception, to_string_method);
    if ((*jni_env)->ExceptionCheck(jni_env))
    {
        (*jni_env)->DeleteLocalRef(jni_env, exception_class);
        (*jni_env)->ExceptionClear(jni_env);
        return -1;
    }
    if (exception_str == NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, exception_class);
        return -1;
    }

    char *str = (char*)(*jni_env)->GetStringUTFChars(jni_env, exception_str, NULL);
    int wrote = snprintf(stack_trace_str, max_stack_trace_lenght, "%s\n", str);
    if (wrote < 0 )
    {   /* this should never happen, snprintf() usually works w/o errors */
        return -1;
    }
    if (wrote > 0 && stack_trace_str[wrote-1] != '\n')
    {
        VERBOSE_PRINT("Too long exception string. Not generating stack trace at all.");
        /* in order to not show partial exception clear current frame's data */
        stack_trace_str[0] = '\0';
        return 0;
    }

    (*jni_env)->ReleaseStringUTFChars(jni_env, exception_str, str);
    (*jni_env)->DeleteLocalRef(jni_env, exception_str);

    jmethodID get_stack_trace_method = (*jni_env)->GetMethodID(jni_env, exception_class, "getStackTrace", "()[Ljava/lang/StackTraceElement;");
    (*jni_env)->DeleteLocalRef(jni_env, exception_class);

    if (get_stack_trace_method == NULL)
    {
        VERBOSE_PRINT("Cannot get getStackTrace() method id");
        return wrote;
    }

    jobject stack_trace_array = (*jni_env)->CallObjectMethod(jni_env, exception, get_stack_trace_method);
    if ((*jni_env)->ExceptionCheck(jni_env))
    {
        (*jni_env)->ExceptionClear(jni_env);
        return wrote;
    }
    if (stack_trace_array ==  NULL)
    {
        return wrote;
    }

    jint array_size = (*jni_env)->GetArrayLength(jni_env, stack_trace_array);
    for (jint i = 0; i < array_size; ++i)
    {
        jobject frame_element = (*jni_env)->GetObjectArrayElement(jni_env, stack_trace_array, i);
        const int frame_wrote = print_stack_trace_element(jvmti_env, jni_env, frame_element, stack_trace_str + wrote, max_stack_trace_lenght - wrote);
        (*jni_env)->DeleteLocalRef(jni_env, frame_element);

        if (frame_wrote <= 0)
        {   /* <  0 : this should never happen, snprintf() usually works w/o errors */
            /* == 0 : wrote nothing: the length limit was reached and no more */
            /* frames can be added to the stack trace */
            break;
        }

        wrote += frame_wrote;
    }

    (*jni_env)->DeleteLocalRef(jni_env, stack_trace_array);

    return wrote;
}

static char *generate_thread_stack_trace(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            char     *thread_name,
            jobject  exception)
{
    char  *stack_trace_str;
    /* allocate string which will contain stack trace */
    stack_trace_str = (char*)calloc(MAX_STACK_TRACE_STRING_LENGTH + 1, sizeof(char));
    if (stack_trace_str == NULL)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": calloc(): out of memory");
        return NULL;
    }

    int wrote = snprintf(stack_trace_str, MAX_STACK_TRACE_STRING_LENGTH, "Exception in thread \"%s\" ", thread_name);
    int exception_wrote = print_exception_stack_trace(jvmti_env, jni_env, exception, stack_trace_str + wrote, MAX_STACK_TRACE_STRING_LENGTH - wrote);
    if (exception_wrote <= 0)
    {
        free(stack_trace_str);
        return NULL;
    }

    wrote += exception_wrote;

    jclass exception_class = (*jni_env)->GetObjectClass(jni_env, exception);
    if (NULL == exception_class)
    {
        VERBOSE_PRINT("Cannot get class of an object\n");
        return stack_trace_str;
    }

    jmethodID get_cause_method = (*jni_env)->GetMethodID(jni_env, exception_class, "getCause", "()Ljava/lang/Throwable;");
    (*jni_env)->DeleteLocalRef(jni_env, exception_class);

    if (NULL == get_cause_method)
    {
        VERBOSE_PRINT("Cannot find get an id of getCause()Ljava/lang/Throwable; method\n");
        return stack_trace_str;
    }

    jobject cause = (*jni_env)->CallObjectMethod(jni_env, exception, get_cause_method);
    while (NULL != cause)
    {
        if ((size_t)(MAX_STACK_TRACE_STRING_LENGTH - wrote) < (sizeof(CAUSED_STACK_TRACE_HEADER) - 1))
        {
            VERBOSE_PRINT(__FILE__ ":" STRINGIZE(__LINE__)": Full exception stack trace buffer. Cannot add a cause.");
            (*jni_env)->DeleteLocalRef(jni_env, cause);
            break;
        }

        strcat(stack_trace_str + wrote, CAUSED_STACK_TRACE_HEADER);
        wrote += sizeof(CAUSED_STACK_TRACE_HEADER) - 1;

        const int cause_wrote = print_exception_stack_trace(jvmti_env, jni_env, cause, stack_trace_str + wrote, MAX_STACK_TRACE_STRING_LENGTH - wrote);

        if (cause_wrote <= 0)
        {   /* <  0 : this should never happen, snprintf() usually works w/o errors */
            /* == 0 : wrote nothing: the length limit was reached and no more */
            /* cause can be added to the stack trace */
            break;
        }

        wrote += cause_wrote;

        jobject next_cause = (*jni_env)->CallObjectMethod(jni_env, cause, get_cause_method);
        (*jni_env)->DeleteLocalRef(jni_env, cause);
        cause = next_cause;
    }

    return stack_trace_str;
}

#ifdef GENERATE_JVMTI_STACK_TRACE
/*
 * Print one method from stack frame.
 */
static void print_one_method_from_stack(
            jvmtiEnv       *jvmti_env,
            JNIEnv         *jni_env,
            jvmtiFrameInfo  stack_frame,
            char           *stack_trace_str)
{
    jvmtiError  error_code;
    jclass      declaring_class;
    char       *method_name = "";
    char       *declaring_class_name = "";

    error_code = (*jvmti_env)->GetMethodName(jvmti_env, stack_frame.method, &method_name, NULL, NULL);
    if (error_code != JVMTI_ERROR_NONE)
    {
        return;
    }
    error_code = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, stack_frame.method, &declaring_class);
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    error_code = (*jvmti_env)->GetClassSignature(jvmti_env, declaring_class, &declaring_class_name, NULL);
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));

    if (error_code != JVMTI_ERROR_NONE)
    {
        return;
    }
    char *updated_class_name = format_class_name_for_JNI_call(declaring_class_name);
    int line_number = get_line_number(jvmti_env, stack_frame.method, stack_frame.location);
    char *source_file_name;
    if (declaring_class != NULL)
    {
        error_code = (*jvmti_env)->GetSourceFileName(jvmti_env, declaring_class, &source_file_name);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }

    char buf[1000];
    char line_number_buf[20];
    if (line_number >= 0)
    {
        sprintf(line_number_buf, "%d", line_number);
    }
    else
    {
        strcpy(line_number_buf, "Unknown location");
    }

    char *class_location = get_path_to_class(jvmti_env, jni_env, declaring_class, updated_class_name, TO_EXTERNAL_FORM_METHOD_NAME);
    sprintf(buf, "\tat %s%s(%s:%s) [%s]\n", updated_class_name, method_name, source_file_name, line_number_buf, class_location == NULL ? "unknown" : class_location);
    free(class_location);
    strncat(stack_trace_str, buf, MAX_STACK_TRACE_STRING_LENGTH - strlen(stack_trace_str) - 1);

#ifdef VERBOSE
    if (line_number >= 0)
    {
        printf("\tat %s%s(%s:%d location)\n", updated_class_name, method_name, source_file_name, line_number);
    }
    else
    {
        printf("\tat %s%s(%s:Unknown location)\n", updated_class_name, method_name, source_file_name);
    }
#endif

    /* cleanup */
    if (method_name != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)method_name);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (declaring_class_name != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)declaring_class_name);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
}
#endif /* GENERATE_JVMTI_STACK_TRACE */


#ifdef GENERATE_JVMTI_STACK_TRACE
/*
 * Print stack trace for given thread.
 */
static char *generate_stack_trace(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            jthread   thread,
            char     *thread_name,
            char     *exception_class_name)
{
    jvmtiError     error_code;
    jvmtiFrameInfo stack_frames[MAX_STACK_TRACE_DEPTH];

    char  *stack_trace_str;
    char  buf[1000];
    int count;
    int i;

    /* allocate string which will contain stack trace */
    stack_trace_str = (char*)calloc(MAX_STACK_TRACE_STRING_LENGTH + 1, sizeof(char));
    if (stack_trace_str == NULL)
    {
        fprintf(stderr, "calloc(): out of memory");
        return NULL;
    }

    /* get stack trace */
    error_code = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, MAX_STACK_TRACE_DEPTH, stack_frames, &count);
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));

    VERBOSE_PRINT("Number of records filled: %d\n", count);

    /* is stack trace empty? */
    if (count < 1)
    {
        free(stack_trace_str);
        return NULL;
    }

    sprintf(buf, "Exception in thread \"%s\" %s\n", thread_name, exception_class_name);
    strncat(stack_trace_str, buf, MAX_STACK_TRACE_STRING_LENGTH - strlen(stack_trace_str) - 1);

    /* print content of stack frames */
    for (i = 0; i < count; i++) {
        jvmtiFrameInfo stack_frame = stack_frames[i];
        print_one_method_from_stack(jvmti_env, jni_env, stack_frame, stack_trace_str);
    }

    VERBOSE_PRINT(
    "Exception Stack Trace\n"
    "=====================\n"
    "Stack Trace Depth: %d\n"
    "%s\n", count, stack_trace_str);


    return stack_trace_str;
}
#endif /* GENERATE_JVMTI_STACK_TRACE */



/**
 * Called when an exception is thrown.
 */
static void JNICALL callback_on_exception(
            jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thr,
            jmethodID method,
            jlocation location __UNUSED_VAR,
            jobject exception_object,
            jmethodID catch_method,
            jlocation catch_location __UNUSED_VAR)
{
    jvmtiError error_code;

    char *method_name_ptr;
    char *method_signature_ptr;
    char *class_name_ptr;
    char *class_signature_ptr;
    char *exception_name_ptr;
    char *updated_exception_name_ptr;

    jclass method_class;
    jclass exception_class;

    /* all operations should be processed in critical section */
    enter_critical_section(jvmti_env);

    char tname[MAX_THREAD_NAME_LENGTH];
    get_thread_name(jvmti_env, thr, tname, sizeof(tname));

    exception_class = (*jni_env)->GetObjectClass(jni_env, exception_object);

    /* retrieve all required informations */
    (*jvmti_env)->GetMethodName(jvmti_env, method, &method_name_ptr, &method_signature_ptr, NULL);
    (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &method_class);
    (*jvmti_env)->GetClassSignature(jvmti_env, method_class, &class_signature_ptr, NULL);
    (*jvmti_env)->GetClassSignature(jvmti_env, exception_class, &exception_name_ptr, NULL);

    /* readable class names */
    class_name_ptr = format_class_name(class_signature_ptr, '.');
    updated_exception_name_ptr = format_class_name(exception_name_ptr, '\0');

    INFO_PRINT("%s %s exception in thread \"%s\" ", (catch_method == NULL ? "Uncaught" : "Caught"), updated_exception_name_ptr, tname);
    INFO_PRINT("in a method %s%s() with signature %s\n", class_name_ptr, method_name_ptr, method_signature_ptr);

    if (catch_method == NULL || exception_is_intended_to_be_reported(updated_exception_name_ptr))
    {
        jlong tid = 0;
        T_jthrowableCircularBuf *threads_exc_buf = NULL;

        if (NULL != threadMap && 0 == get_tid(jni_env, thr, &tid))
        {
            threads_exc_buf = jthread_map_get(threadMap, tid);
            VERBOSE_PRINT("Got circular buffer for thread %p\n", (void *)threads_exc_buf);
        }
        else
        {
            VERBOSE_PRINT("Cannot get thread's ID. Disabling reporting to ABRT.");
        }

        if (NULL == threads_exc_buf || NULL == jthrowable_circular_buf_find(threads_exc_buf, exception_object))
        {
            if (NULL != threads_exc_buf)
            {
                VERBOSE_PRINT("Pushing to circular buffer\n");
                jthrowable_circular_buf_push(threads_exc_buf, exception_object);
            }

            log_print("%s %s exception in thread \"%s\" ", (catch_method == NULL ? "Uncaught" : "Caught"), updated_exception_name_ptr, tname);
            log_print("in a method %s%s() with signature %s\n", class_name_ptr, method_name_ptr, method_signature_ptr);

            //char *stack_trace_str = generate_stack_trace(jvmti_env, jni_env, thr, tname, updated_exception_name_ptr);
            char *stack_trace_str = generate_thread_stack_trace(jvmti_env, jni_env, tname, exception_object);
            if (NULL != stack_trace_str)
            {
                log_print("%s", stack_trace_str);
                if (NULL != threads_exc_buf)
                {
                    register_abrt_event(processProperties.main_class, (catch_method == NULL ? "Uncaught exception" : "Caught exception"), (unsigned char *)method_name_ptr, stack_trace_str);
                }
                free(stack_trace_str);
            }
        }
        else
        {
            VERBOSE_PRINT("The exception was already reported!\n");
        }
    }

    /* cleapup */
    if (method_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (method_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (class_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (exception_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)exception_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }

    exit_critical_section(jvmti_env);
}



/*
 * This function is called when an exception is catched.
 */
static void JNICALL callback_on_exception_catch(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env __UNUSED_VAR,
            jthread   thr __UNUSED_VAR,
            jmethodID method,
            jlocation location __UNUSED_VAR,
            jobject   exception __UNUSED_VAR)
{
    jvmtiError error_code;

    char *method_name_ptr;
    char *method_signature_ptr;
    char *class_signature_ptr;

    jclass class;

    /* all operations should be processed in critical section */
    enter_critical_section(jvmti_env);

    /* retrieve all required informations */
    (*jvmti_env)->GetMethodName(jvmti_env, method, &method_name_ptr, &method_signature_ptr, NULL);
    (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &class);
    (*jvmti_env)->GetClassSignature(jvmti_env, class, &class_signature_ptr, NULL);

#ifdef VERBOSE
    /* readable class name */
    char *class_name_ptr = format_class_name(class_signature_ptr, '.');
#endif

    VERBOSE_PRINT("An exception was caught in a method %s%s() with signature %s\n", class_name_ptr, method_name_ptr, method_signature_ptr);

    /* cleapup */
    if (method_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (method_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }
    if (class_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(__LINE__));
    }

    exit_critical_section(jvmti_env);
}



/**
 * Called when an object is allocated.
 */
static void JNICALL callback_on_object_alloc(
            jvmtiEnv *jvmti_env,
            JNIEnv* jni_env __UNUSED_VAR,
            jthread thread __UNUSED_VAR,
            jobject object __UNUSED_VAR,
            jclass object_klass,
            jlong size)
{
    char *signature_ptr;

    enter_critical_section(jvmti_env);
    (*jvmti_env)->GetClassSignature(jvmti_env, object_klass, &signature_ptr, NULL);

    if (size >= VM_MEMORY_ALLOCATION_THRESHOLD)
    {
        INFO_PRINT("object allocation: instance of class %s, allocated %ld bytes\n", signature_ptr, (long int)size);
    }
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)signature_ptr);
    exit_critical_section(jvmti_env);
}



/**
 * Called when an object is freed.
 */
static void JNICALL callback_on_object_free(
            jvmtiEnv *jvmti_env,
            jlong tag __UNUSED_VAR)
{
    enter_critical_section(jvmti_env);
    VERBOSE_PRINT("object free\n");
    exit_critical_section(jvmti_env);
}



/**
 * Called on GC start.
 */
static void JNICALL callback_on_gc_start(
            jvmtiEnv *jvmti_env)
{
    enter_critical_section(jvmti_env);
    gc_start_time = clock();
    VERBOSE_PRINT("GC start\n");
    exit_critical_section(jvmti_env);
}



/**
 * Called on GC finish.
 */
static void JNICALL callback_on_gc_finish(
            jvmtiEnv *jvmti_env)
{
    clock_t gc_end_time = clock();
    int diff;
    enter_critical_section(jvmti_env);
    INFO_PRINT("GC end\n");
    diff = (gc_end_time - (gc_start_time))/CLOCKS_PER_SEC;
    if (diff > GC_TIME_THRESHOLD)
    {
        char str[100];
        sprintf(str, "GC took more time than expected: %d\n", diff);
        INFO_PRINT("%s\n", str);
        register_abrt_event(processProperties.main_class, str, (unsigned char *)"GC thread", "no stack trace");
    }
    exit_critical_section(jvmti_env);
}



/**
 * Called when some method is about to be compiled.
 */
static void JNICALL callback_on_compiled_method_load(
            jvmtiEnv   *jvmti_env,
            jmethodID   method,
            jint        code_size __UNUSED_VAR,
            const void *code_addr __UNUSED_VAR,
            jint        map_length __UNUSED_VAR,
            const jvmtiAddrLocationMap* map __UNUSED_VAR,
            const void  *compile_info __UNUSED_VAR)
{
    jvmtiError error_code;
    char* name = NULL;
    char* signature = NULL;
    char* generic_ptr = NULL;
    char* class_signature = NULL;
    jclass class;

    enter_critical_section(jvmti_env);

    error_code = (*jvmti_env)->GetMethodName(jvmti_env, method, &name, &signature, &generic_ptr);
    check_jvmti_error(jvmti_env, error_code, "get method name");

    error_code = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &class);
    check_jvmti_error(jvmti_env, error_code, "get method declaring class");
    (*jvmti_env)->GetClassSignature(jvmti_env, class, &class_signature, NULL);

    INFO_PRINT("Compiling method: %s.%s with signature %s %s   Code size: %5d\n",
        class_signature == NULL ? "" : class_signature,
        name, signature,
        generic_ptr == NULL ? "" : generic_ptr, (int)code_size);

    if (name != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)name);
        check_jvmti_error(jvmti_env, error_code, "deallocate name");
    }
    if (signature != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)signature);
        check_jvmti_error(jvmti_env, error_code, "deallocate signature");
    }
    if (generic_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)generic_ptr);
        check_jvmti_error(jvmti_env, error_code, "deallocate generic_ptr");
    }
    if (class_signature != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)class_signature);
        check_jvmti_error(jvmti_env, error_code, "deallocate class_signature");
    }

    exit_critical_section(jvmti_env);
}



/*
 * Sel all required JVMTi capabilities.
 */
jvmtiError set_capabilities(jvmtiEnv *jvmti_env)
{
    jvmtiCapabilities capabilities;
    jvmtiError error_code;

    /* Add JVMTI capabilities */
    (void)memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    capabilities.can_signal_thread = 1;
    capabilities.can_get_owned_monitor_info = 1;
    capabilities.can_generate_method_entry_events = 1;
    capabilities.can_generate_method_exit_events = 1;
    capabilities.can_generate_frame_pop_events = 1;
    capabilities.can_generate_exception_events = 1;
    capabilities.can_generate_vm_object_alloc_events = 1;
    capabilities.can_generate_object_free_events = 1;
    capabilities.can_generate_garbage_collection_events = 1;
    capabilities.can_generate_compiled_method_load_events = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_tag_objects = 1;

    error_code = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
    check_jvmti_error(jvmti_env, error_code, "Unable to get necessary JVMTI capabilities.");
    return error_code;
}



/*
 * Register all callback functions.
 */
jvmtiError register_all_callback_functions(jvmtiEnv *jvmti_env)
{
    jvmtiEventCallbacks callbacks;
    jvmtiError error_code;

    /* Initialize callbacks structure */
    (void)memset(&callbacks, 0, sizeof(callbacks));

    /* JVMTI_EVENT_VM_INIT */
    callbacks.VMInit = &callback_on_vm_init;

    /* JVMTI_EVENT_VM_DEATH */
    callbacks.VMDeath = &callback_on_vm_death;

    /* JVMTI_EVENT_THREAD_START */
    callbacks.ThreadStart = &callback_on_thread_start;

    /* JVMTI_EVENT_THREAD_END */
    callbacks.ThreadEnd = &callback_on_thread_end;

    /* JVMTI_EVENT_EXCEPTION */
    callbacks.Exception = &callback_on_exception;

    /* JVMTI_EVENT_EXCEPTION_CATCH */
    callbacks.ExceptionCatch = &callback_on_exception_catch;

    /* JVMTI_EVENT_VM_OBJECT_ALLOC */
    callbacks.VMObjectAlloc = &callback_on_object_alloc;

    /* JVMTI_EVENT_OBJECT_FREE */
    callbacks.ObjectFree = &callback_on_object_free;

    /* JVMTI_EVENT_GARBAGE_COLLECTION_START */
    callbacks.GarbageCollectionStart  = &callback_on_gc_start;

    /* JVMTI_EVENT_GARBAGE_COLLECTION_FINISH */
    callbacks.GarbageCollectionFinish = &callback_on_gc_finish;

    /* JVMTI_EVENT_COMPILED_METHOD_LOAD */
    callbacks.CompiledMethodLoad = &callback_on_compiled_method_load;

    error_code = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, (jint)sizeof(callbacks));
    check_jvmti_error(jvmti_env, error_code, "Cannot set jvmti callbacks");
    return error_code;
}



/*
 * Set given event notification mode.
 */
jvmtiError set_event_notification_mode(jvmtiEnv* jvmti_env, int event)
{
    jvmtiError error_code;

    error_code = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, event, (jthread)NULL);
    check_jvmti_error(jvmti_env, error_code, "Cannot set event notification");
    return error_code;
}



/*
 * Configure all event notification modes.
 */
jvmtiError set_event_notification_modes(jvmtiEnv* jvmti_env)
{
    jvmtiError error_code;

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_VM_INIT)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_VM_DEATH)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_THREAD_START)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_THREAD_END)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_VM_OBJECT_ALLOC)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_EXCEPTION)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_EXCEPTION_CATCH)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_VM_OBJECT_ALLOC)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_OBJECT_FREE)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_GARBAGE_COLLECTION_START)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code= set_event_notification_mode(jvmti_env, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH)) != JNI_OK)
    {
        return error_code;
    }

    if ((error_code = set_event_notification_mode(jvmti_env, JVMTI_EVENT_COMPILED_METHOD_LOAD)) != JNI_OK)
    {
        return error_code;
    }

    return error_code;
}



/*
 * Create monitor used to acquire and free global lock (mutex).
 */
jvmtiError create_raw_monitor(jvmtiEnv *jvmti_env)
{
    jvmtiError error_code;

    error_code = (*jvmti_env)->CreateRawMonitor(jvmti_env, "agent data", &lock);
    check_jvmti_error(jvmti_env, error_code, "Cannot create raw monitor");

    return error_code;
}



/*
 * Print major, minor and micro version of JVM TI.
 */
jvmtiError print_jvmti_version(jvmtiEnv *jvmti_env __UNUSED_VAR)
{
#ifndef SILENT
    jvmtiError error_code;

    jint version;
    jint cmajor, cminor, cmicro;

    error_code = (*jvmti_env)->GetVersionNumber(jvmti_env, &version);

    cmajor = (version & JVMTI_VERSION_MASK_MAJOR) >> JVMTI_VERSION_SHIFT_MAJOR;
    cminor = (version & JVMTI_VERSION_MASK_MINOR) >> JVMTI_VERSION_SHIFT_MINOR;
    cmicro = (version & JVMTI_VERSION_MASK_MICRO) >> JVMTI_VERSION_SHIFT_MICRO;
    printf("Compile Time JVMTI Version: %d.%d.%d (0x%08x)\n", cmajor, cminor, cmicro, version);

    return error_code;
#else
    return 0;
#endif
}



/*
 * Returns NULL-terminated char *vector[]. Result itself must be freed,
 * but do no free list elements. IOW: do free(result), but never free(result[i])!
 * If separated_list is NULL or "", returns NULL.
 */
static char **build_string_vector(const char *separated_list, char separator)
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
            cnt += (*cp++ == separator);

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
        while (*p)
        {
            if (*p++ == separator)
            {
                /* Replace 'separator' by '\0' */
                p[-1] = '\0';
                /* Save pointer to the beginning of next string in the pointer region */
                *pp++ = p;
            }
        }
    }

    return vector;
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
void parse_commandline_options(char *options)
{
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
        if (strcmp("abrt", key) == 0)
        {
            if (value != NULL && (strcasecmp("on", value) == 0 || strcasecmp("yes", value) == 0))
            {
                VERBOSE_PRINT("Enabling errors reporting to ABRT\n");
                reportErrosTo |= ED_ABRT;
            }
        }
        else if(strcmp("output", key) == 0)
        {
            if (value == NULL || value[0] == '\0')
            {
                VERBOSE_PRINT("Disabling output to log file\n");
                outputFileName = DISABLED_LOG_OUTPUT;
            }
            else
            {
                outputFileName = strdup(value);
                if (outputFileName == NULL)
                {
                    fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": strdup(output): out of memory\n");
                    VERBOSE_PRINT("Can not configure output file to desired value\n");
                    /* keep NULL in outputFileName -> the default name will be used */
                }
            }
        }
        else if(strcmp("caught", key) == 0)
        {
            reportedCaughExceptionTypes = build_string_vector(value, ':');
        }
        else
        {
            fprintf(stderr, "Unknow option '%s'\n", key);
        }
    }
}



/*
 * Called when agent is loading into JVM.
 */
JNIEXPORT jint JNICALL Agent_OnLoad(
        JavaVM *jvm,
        char *options,
        void *reserved __UNUSED_VAR)
{
    jvmtiEnv  *jvmti_env = NULL;
    jvmtiError error_code = JVMTI_ERROR_NONE;
    jint       result;

    INFO_PRINT("Agent_OnLoad\n");
    VERBOSE_PRINT("VERBOSE OUTPUT ENABLED\n");
    parse_commandline_options(options);

    /* check if JVM TI version is correct */
    result = (*jvm)->GetEnv(jvm, (void **) &jvmti_env, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti_env == NULL)
    {
        fprintf(stderr, "ERROR: Unable to access JVMTI Version 1 (0x%x),"
                " is your J2SE a 1.5 or newer version? JNIEnv's GetEnv() returned %d which is wrong.\n",
                JVMTI_VERSION_1, (int)result);
        return result;
    }
    INFO_PRINT("JVM TI version is correct\n");

    print_jvmti_version(jvmti_env);

    /* set required JVM TI agent capabilities */
    if ((error_code = set_capabilities(jvmti_env)) != JNI_OK)
    {
        return error_code;
    }

    /* register all callback functions */
    if ((error_code = register_all_callback_functions(jvmti_env)) != JNI_OK)
    {
        return error_code;
    }

    /* set notification modes for all callback functions */
    if ((error_code = set_event_notification_modes(jvmti_env)) != JNI_OK)
    {
        return error_code;
    }

    /* create global mutex */
    if ((error_code = create_raw_monitor(jvmti_env)) != JNI_OK)
    {
        return error_code;
    }

    /* if output log file is not disabled */
    if (outputFileName != DISABLED_LOG_OUTPUT)
    {
        /* open output log file */
        const char *fn = (outputFileName != NULL ? outputFileName : get_default_log_file_name());
        VERBOSE_PRINT("Path to the log file: %s\n", fn);
        fout = fopen(fn, "wt");
        if (fout == NULL)
        {
            fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": can not create output file %s\n", fn);
            return -1;
        }
    }

    threadMap = jthread_map_new();
    if (NULL == threadMap)
    {
        fprintf(stderr, __FILE__ ":" STRINGIZE(__LINE__) ": can not create a set of reported exceptions\n");
        return -1;
    }

    return JNI_OK;
}



/*
 * Called when agent is unloading from JVM.
 */
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm __UNUSED_VAR)
{
    INFO_PRINT("Agent_OnUnLoad\n");
    if (outputFileName != DISABLED_LOG_OUTPUT)
    {
        free(outputFileName);
    }

    free(reportedCaughExceptionTypes);

    if (fout != NULL)
    {
        fclose(fout);
    }

    jthread_map_free(threadMap);
}



/*
 * finito
 */

