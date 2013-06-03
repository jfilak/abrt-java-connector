/*
 * Simple JVMTI Agent demo.
 *
 * Pavel Tisnovsky <ptisnovs@redhat.com>
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
#include <unistd.h>
#include <linux/limits.h>

/* Macros used to convert __LINE__ into string */
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define __UNUSED_VAR __attribute__ ((unused))

/* ABRT include file */
#if REPORT_ERRORS_TO_ABRT == 1
#include "internal_libabrt.h"
#else
#warning "Building version without errors reporting"
#endif

/* JVM TI include files */
#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>




/* Basic settings */
#undef VERBOSE
#define VM_MEMORY_ALLOCATION_THRESHOLD 1024
#define GC_TIME_THRESHOLD 1

/* For debugging purposes */
#define PRINT_JVM_ENVIRONMENT_VARIABLES 1

/* Don't need to be changed */
#define MAX_THREAD_NAME_LENGTH 40

/* Name of file created by the agent */
#define OUTPUT_FILE_NAME "agent.log"

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



/* forward headers */
static char* get_path_to_class(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass class, char *class_name, const char *stringize_method_name);
static void print_jvm_environment_variables_to_file(FILE *out);



/*
 * Return PID (process ID) as a string.
 */
static void get_pid_as_string(char * buffer)
{
    int pid = getpid();
    sprintf(buffer, "%d", pid);
    puts(buffer);
}



/*
 * Return UID (user ID) as a string.
 */
static void get_uid_as_string(char * buffer)
{
    int uid = getuid();
    sprintf(buffer, "%d", uid);
    puts(buffer);
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
 * Add JVM environment data into ABRT event message.
 */
#if REPORT_ERRORS_TO_ABRT == 1
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
#endif



/*
 * Add process properties into ABRT event message.
 */
#if REPORT_ERRORS_TO_ABRT == 1
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
#endif



/*
 * Register new ABRT event using given message and a method name.
 * If REPORT_ERRORS_TO_ABRT is set to zero, this function does nothing.
 */
static void register_abrt_event(char * executable, char * message, unsigned char * method, char * backtrace)
{
#if REPORT_ERRORS_TO_ABRT == 1
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
    printf("problem data created: '%s'\n", res ? "failure" : "success");
    problem_data_free(pd);
#endif
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
    printf("ERROR: JVMTI: %d(%s): %s\n", error_code, msg_err, msg_str);
}



/*
 * Format class signature into a printable form.
 * Class names has form "Ljava/lang/String;"
 * Requested form       "java.lang.String"
 */
char* format_class_name(char *class_signature, char replace_to)
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
 * Class names has form "Ljava/lang/String;"
 * Requested form       "java/lang/String"
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
static void check_jvmti_error(
            jvmtiEnv   *jvmti_env,
            jvmtiError  error_code,
            const char *str)
{
    if ( error_code != JVMTI_ERROR_NONE )
    {
        print_jvmti_error(jvmti_env, error_code, str);
    }
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
            printf("(get_thread_name) Error expected: %d, got: %d\n", JVMTI_ERROR_NONE, error);
            printf("\n");
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
    if (fin == NULL)
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



static void replace_dots_by_slashes(char *class_name)
{
    char *c=class_name;
    for (; *c; c++)
    {
        if (*c=='.') *c='/';
    }
}



static char * create_updated_class_name(char *class_name)
{
    char *upd_class_name = (char*)malloc(strlen(class_name)+1);
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
    replace_dots_by_slashes(class_name);

    jclass cls = (*jni_env)->FindClass(jni_env, class_name);

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
    printf("%-30s: %d\n", "pid", processProperties.pid);
    printf("%-30s: %s\n", "executable", processProperties.executable);
    printf("%-30s: %s\n", "exec_command", processProperties.exec_command);
    printf("%-30s: %s\n", "main_class", processProperties.main_class);
}



/*
 * Print JVM environment
 */
static void print_jvm_environment_variables(void)
{
    print_jvm_environment_variables_to_file(stdout);
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

    printf("Got VM init event\n");
    get_thread_name(jvmti_env , thread, tname, sizeof(tname));
    printf("callbackVMInit:  %s thread\n", tname);

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
    printf("Got VM Death event\n");
    exit_critical_section(jvmti_env);
}



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

    if (method == NULL)
    {
        return -1;
    }

    /* read table containing line numbers and instruction indexes */
    error_code = (*jvmti_env)->GetLineNumberTable(jvmti_env, method, &count, &location_table);
    /* it is possible, that we are unable to read the table -> missing debuginfo etc. */
    if (error_code != JVMTI_ERROR_NONE)
    {
        return -1;
    }

    /* iterate over the read table */
    for (i = 0; i < count - 1; i++)
    {
        jvmtiLineNumberEntry entry1 = location_table[i];
        jvmtiLineNumberEntry entry2 = location_table[i+1];
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

    char *upd_class_name = (char*)malloc(strlen(class_name)+7);
    strcpy(upd_class_name, class_name);
    strcat(upd_class_name, "class");

    /* find ClassLoader class */
    class_loader_class = (*jni_env)->FindClass(jni_env, "java/lang/ClassLoader");
    if (class_loader_class ==  NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    /* find method ClassLoader.getResource() */
    jmethodID get_resource = (*jni_env)->GetMethodID(jni_env, class_loader_class, "getResource", "(Ljava/lang/String;)Ljava/net/URL;" );
    if (get_resource ==  NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        return NULL;
    }

    /* convert new class name into a Java String */
    jstring j_class_name = (*jni_env)->NewStringUTF(jni_env, upd_class_name);

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

    /* call method URL.toString() */
    jstring jstr = (jstring)(*jni_env)->CallObjectMethod(jni_env, url, to_external_form);
    if (jstr ==  NULL)
    {
        (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
        (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
        return NULL;
    }

    /* convert Java String into C char* */
    char *str = (char*)(*jni_env)->GetStringUTFChars(jni_env, jstr, NULL);
    char *out = (char*)calloc(strlen(str)+1, sizeof(char));
    strcpy(out, str);

    /* cleanup */
    (*jni_env)->ReleaseStringUTFChars(jni_env, jstr, str);
    (*jni_env)->DeleteLocalRef(jni_env, class_loader_class);
    (*jni_env)->DeleteLocalRef(jni_env, j_class_name);
    return out;
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
        return NULL;
    }
    else
    {
        return get_path_to_class_class_loader(jvmti_env, jni_env, class_loader, class_name, stringize_method_name);
    }
}



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
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    error_code = (*jvmti_env)->GetClassSignature(jvmti_env, declaring_class, &declaring_class_name, NULL);
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));

    if (error_code != JVMTI_ERROR_NONE)
    {
        return;
    }
    char *updated_class_name = format_class_name_for_JNI_call(declaring_class_name);
    int line_number = get_line_number(jvmti_env, stack_frame.method, stack_frame.location);
    char *source_file_name;
    if (declaring_class != NULL)
    {
        (*jvmti_env)->GetSourceFileName(jvmti_env, declaring_class, &source_file_name);
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
        printf("\tat %s%s(%s:Unknown location)\n", updated_class_name, source_file_name, method_name_ptr);
    }
#endif

    /* cleanup */
    if (method_name != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)method_name);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (declaring_class_name != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)declaring_class_name);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
}



/*
 * Print stack trace for given thread.
 */
static void print_stack_trace(
            jvmtiEnv *jvmti_env,
            JNIEnv   *jni_env,
            jthread   thread,
            char     *original_method_name)
{
    jvmtiError     error_code;
    jvmtiFrameInfo stack_frames[MAX_STACK_TRACE_DEPTH];

    char  *stack_trace_str;
    int count;
    int i;

    /* allocate string which will contain stack trace */
    stack_trace_str = (char*)calloc(MAX_STACK_TRACE_STRING_LENGTH + 1, sizeof(char));

    /* get stack trace */
    error_code = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, MAX_STACK_TRACE_DEPTH, stack_frames, &count);
    check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));

#ifdef VERBOSE
	    printf("Number of records filled: %d\n", count);
#endif

    /* is stack trace empty? */
    if (count < 1)
    {
        return;
    }

#ifdef VERBOSE
    printf("Exception Stack Trace\n");
    printf("=====================\n");
    printf("Stack Trace Depth: %d\n", count); 
#endif

    /* print content of stack frames */
    for (i = 0; i < count; i++) {
        jvmtiFrameInfo stack_frame = stack_frames[i];
        print_one_method_from_stack(jvmti_env, jni_env, stack_frame, stack_trace_str);
    }

    puts(stack_trace_str);

    fprintf(fout, "Exception Stack Trace\n");
    fprintf(fout, "=====================\n");
    fprintf(fout, "Stack Trace Depth: %d\n", count); 
    fprintf(fout, "%s\n", stack_trace_str);

    register_abrt_event(processProperties.main_class, "Uncaught exception", (unsigned char *)original_method_name, stack_trace_str);
    free(stack_trace_str);
}



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

    if (catch_method == NULL)
    {
        fprintf(fout, "Uncaught exception in thread \"%s\" ", tname);
        printf("Uncaught exception in thread \"%s\" ", tname);
    }
    else
    {
        fprintf(fout, "Caught exception in thread \"%s\" ", tname);
        printf("Caught exception in thread \"%s\" ", tname);
    }

    /* retrieve all required informations */
    (*jvmti_env)->GetMethodName(jvmti_env, method, &method_name_ptr, &method_signature_ptr, NULL);
    (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &method_class);
    (*jvmti_env)->GetClassSignature(jvmti_env, method_class, &class_signature_ptr, NULL);
    (*jvmti_env)->GetClassSignature(jvmti_env, exception_class, &exception_name_ptr, NULL);

    /* readable class names */
    class_name_ptr = format_class_name(class_signature_ptr, '.');
    updated_exception_name_ptr = format_class_name(exception_name_ptr, '\0');

    fprintf(fout, "in a method %s%s() with signature %s\n", class_name_ptr, method_name_ptr, method_signature_ptr);
    printf("%s\n", updated_exception_name_ptr);

    if (catch_method == NULL)
    {
        print_stack_trace(jvmti_env, jni_env, thr, method_name_ptr);
    }
    else
    {
        char *exception_signature;
        jclass class = (*jni_env)->GetObjectClass(jni_env, exception_object);
        (*jvmti_env)->GetClassSignature(jvmti_env, class, &exception_signature, NULL);
        /* special cases for selected exceptions */
        if (strcmp("Ljava/io/FileNotFoundException;", exception_signature)==0)
        {
            register_abrt_event(processProperties.main_class, "Caught exception: file not found", (unsigned char *)method_name_ptr, "");
        }
        fprintf(fout, "exception object is: %s\n", exception_signature);
        (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)exception_signature);
    }

    /* cleapup */
    if (method_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (method_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (class_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (exception_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)exception_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }

    exit_critical_section(jvmti_env);
}



/*
 * This function is called when an exception is catched.
 */
static void JNICALL callback_on_exception_catch(
            jvmtiEnv *jvmti_env,
            JNIEnv   *env __UNUSED_VAR,
            jthread   thr __UNUSED_VAR,
            jmethodID method,
            jlocation location __UNUSED_VAR,
            jobject   exception __UNUSED_VAR)
{
    jvmtiError error_code;

    char *method_name_ptr;
    char *method_signature_ptr;
    char *class_name_ptr;
    char *class_signature_ptr;

    jclass class;

    /* all operations should be processed in critical section */
    enter_critical_section(jvmti_env);

    /* retrieve all required informations */
    (*jvmti_env)->GetMethodName(jvmti_env, method, &method_name_ptr, &method_signature_ptr, NULL);
    (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &class);
    (*jvmti_env)->GetClassSignature(jvmti_env, class, &class_signature_ptr, NULL);

    /* readable class name */
    class_name_ptr = format_class_name(class_signature_ptr, '.');

#ifdef VERBOSE
    printf("An exception was caught in a method %s with signature %s\n", method_name_ptr, method_signature_ptr);
#endif
    fprintf(fout,"An exception was caught in a method %s%s() with signature %s\n", class_name_ptr, method_name_ptr, method_signature_ptr); 

    /* cleapup */
    if (method_name_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (method_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
    }
    if (class_signature_ptr != NULL)
    {
        error_code = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_signature_ptr);
        check_jvmti_error(jvmti_env, error_code, __FILE__ ":" STRINGIZE(_LINE__));
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
        printf("object allocation: instance of class %s, allocated %ld bytes\n", signature_ptr, (long int)size);
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
#ifdef VERBOSE
    printf("object free\n");
#endif
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
#ifdef VERBOSE
    printf("GC start\n");
#endif
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
#ifdef VERBOSE
    printf("GC end\n");
#endif
    diff = (gc_end_time - (gc_start_time))/CLOCKS_PER_SEC;
    if (diff > GC_TIME_THRESHOLD)
    {
        char str[100];
        sprintf(str, "GC took more time than expected: %d\n", diff);
        puts(str);
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
            jint        code_size,
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

    fprintf(fout, "Compiling method: %s.%s with signature %s %s   Code size: %5d\n",
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
jvmtiError print_jvmti_version(jvmtiEnv *jvmti_env)
{
    jvmtiError error_code;

    jint version;
    jint cmajor, cminor, cmicro;

    error_code = (*jvmti_env)->GetVersionNumber(jvmti_env, &version);

    cmajor = (version & JVMTI_VERSION_MASK_MAJOR) >> JVMTI_VERSION_SHIFT_MAJOR;
    cminor = (version & JVMTI_VERSION_MASK_MINOR) >> JVMTI_VERSION_SHIFT_MINOR;
    cmicro = (version & JVMTI_VERSION_MASK_MICRO) >> JVMTI_VERSION_SHIFT_MICRO;
    printf("Compile Time JVMTI Version: %d.%d.%d (0x%08x)\n", cmajor, cminor, cmicro, version);

    return error_code;
}



/*
 * Called when agent is loading into JVM.
 */
JNIEXPORT jint JNICALL Agent_OnLoad(
        JavaVM *jvm,
        char *options __UNUSED_VAR,
        void *reserved __UNUSED_VAR)
{
    jvmtiEnv  *jvmti_env = NULL;
    jvmtiError error_code = JVMTI_ERROR_NONE;
    jint       result;

    printf("Agent_OnLoad\n");

    /* check if JVM TI version is correct */
    result = (*jvm)->GetEnv(jvm, (void **) &jvmti_env, JVMTI_VERSION_1_0);
    if (result != JNI_OK || jvmti_env == NULL)
    {
        printf("ERROR: Unable to access JVMTI Version 1 (0x%x),"
                " is your J2SE a 1.5 or newer version? JNIEnv's GetEnv() returned %d which is wrong.\n",
                JVMTI_VERSION_1, (int)result);
        return result;
    }
    puts("JVM TI version is correct");

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

    /* open output log file */
    fout = fopen(OUTPUT_FILE_NAME, "wt");
    if (fout == NULL)
    {
        printf("ERROR: Can not create output file %s\n", OUTPUT_FILE_NAME);
        return -1;
    }

    return JNI_OK;
}



/*
 * Called when agent is unloading from JVM.
 */
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm __UNUSED_VAR)
{
    printf("Agent_OnUnLoad\n");
    if (fout != NULL)
    {
        fclose(fout);
    }
}



/*
 * finito
 */

