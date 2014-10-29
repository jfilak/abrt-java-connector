project(test)

set(HTTP_DIR "/var/www/html")
set(SERVER_URL "http://localhost")

find_package(Java REQUIRED)

set(JAVA_TESTOUTPUT_VER java-${Java_VERSION_MAJOR}.${Java_VERSION_MINOR})
message("Requires testsuite ver: ${JAVA_TESTOUTPUT_VER}")

subdirs(outputs unittests)

function(_get_target_names variable)
    foreach(tmp_name ${ARGN})
        set(${variable} ${CMAKE_CURRENT_BINARY_DIR}/${tmp_name}.class PARENT_SCOPE)
    endforeach(tmp_name)
endfunction()

function(_add_class_target class_name target_list)
    _get_target_names(CLASS_DEPENCIES ${ARGN})
    _get_target_names(target_name ${class_name})
    set(${target_list} ${${target_list}} ${target_name} PARENT_SCOPE)
    add_custom_command(
        OUTPUT ${target_name}
        COMMAND ${Java_JAVAC_EXECUTABLE} -cp ${CMAKE_CURRENT_BINARY_DIR} -d ${CMAKE_CURRENT_BINARY_DIR} ${class_name}.java
        DEPENDS ${CLASS_DEPENCIES} ${class_name}.java
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

function(_add_jar_target base_name output)
    _get_target_names(CLASS_DEPENCIES ${ARGN})

    foreach(tmp_name ${ARGN})
        if (NOT ENTRYPOINT)
            set(ENTRYPOINT ${tmp_name})
        endif()

        set(CONTENTS ${CONTENTS} ${tmp_name}.class)
    endforeach(tmp_name)

    set(real_path ${CMAKE_CURRENT_BINARY_DIR}/${base_name}.jar)
    set(${output} ${real_path} PARENT_SCOPE)
    add_custom_command(
        OUTPUT ${real_path}
        COMMAND ${Java_JAR_EXECUTABLE} cvfe ${base_name}.jar ${ENTRYPOINT} ${CONTENTS}
        DEPENDS ${CLASS_DEPENCIES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

_add_class_target(SimpleTest TEST_JAVA_TARGETS)
_add_class_target(RemoteTest TEST_JAVA_TARGETS SimpleTest)
_add_class_target(ThreadCaughtException TEST_JAVA_TARGETS SimpleTest)
_add_class_target(ThreadUncaughtException TEST_JAVA_TARGETS SimpleTest)
_add_class_target(MultiThreadTest TEST_JAVA_TARGETS SimpleTest ThreadCaughtException ThreadUncaughtException)
_add_class_target(Test TEST_JAVA_TARGETS)
_add_class_target(com/redhat/abrt/test/Test TEST_JAVA_TARGETS)
_add_class_target(com/redhat/abrt/test/UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames TEST_JAVA_TARGETS)
_add_class_target(BadClassTest TEST_JAVA_TARGETS)
_add_class_target(MissingClass TEST_JAVA_TARGETS)
_add_class_target(MissingClassTest TEST_JAVA_TARGETS MissingClass)
_add_class_target(TryFinallyTest TEST_JAVA_TARGETS SimpleTest)
_add_class_target(InnerExceptions TEST_JAVA_TARGETS SimpleTest)
_add_class_target(OverridenEqualExceptionTest TEST_JAVA_TARGETS)
_add_class_target(NoException TEST_JAVA_TARGETS)
_add_class_target(ThreadStressTest TEST_JAVA_TARGETS SimpleTest)
_add_class_target(DataMethodTest TEST_JAVA_TARGETS)

_add_jar_target(JarTest JAR_TEST_PATH SimpleTest ThreadCaughtException ThreadUncaughtException MultiThreadTest)
set(REMOTE_JAR_PATH ${HTTP_DIR}/JarTest.jar)

add_custom_target(AllTestClasses DEPENDS ${TEST_JAVA_TARGETS})

get_property(AGENT_NAME TARGET AbrtChecker PROPERTY OUTPUT_NAME)

#
# Creates a new test target which runs java with abrt-java-connector agent enabled.
# The target will create `target_name`.log file in CMAKE_CURRENT_BINARY_DIR
#
# Help:
#
#   to run test with errors reporting to ABRT issue a command like this:
#
#   $ make run ABRT_ENABLED="on"
#
# Call syntax:
#
#   _add_test_target(target_name
#        [java_param] ... [java_param]
#        PRE [cmd] [arg] ... [arg]
#        DEPENDS [dependency] ... [dependency])
#        AGENT_OPTIONS [key=value] ... [key=value]
#
#        The shell command specified in the PRE param will be run before java
#
function(_add_test_target target_name)
    set(current_var java_params)
    foreach(arg_val ${ARGN})
        if(arg_val STREQUAL "PRE")
            set(current_var pre_command)
        elseif(arg_val STREQUAL "DEPENDS")
            set(current_var depends)
        elseif(arg_val STREQUAL "AGENT_OPTIONS")
            set(current_var agent_options)
        else()
            set(${current_var} ${${current_var}} ${arg_val})
        endif()
    endforeach(arg_val)

    get_directory_property(all_run_targets ALL_RUN_TARGETS)
    set_property(DIRECTORY PROPERTY ALL_RUN_TARGETS ${all_run_targets} ${target_name})

    add_custom_target(
        ${target_name}
        ${pre_command}
# -Xdebug -Xrunjdwp:transport=dt_socket,address=8001,server=y,suspend=n + jdb -attach 8001
        COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="${agent_options},abrt=$$ABRT_ENABLED,output=${target_name}.log" ${java_params}
        DEPENDS AbrtChecker ${depends}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

function(_add_test target_name expected_exit_code)
    add_test(test_${target_name} /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/testdriver ${target_name} ${expected_exit_code} ${CMAKE_CURRENT_BINARY_DIR}/outputs/${target_name}.log ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.log ${ARGN})
endfunction()

function(_add_analyze_test target_name)
    add_test(test_${target_name} /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/analysis_testdriver ${CMAKE_BINARY_DIR}/utils ${test_BINARY_DIR}/outputs/backtrace_${target_name}.log ${test_BINARY_DIR}/${target_name}.log ${test_BINARY_DIR}/outputs/${target_name}.log)
endfunction()

_add_test_target(
    run
    SimpleTest
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException,debugmethod=DataMethodTest.debugStringData
)
_add_test(run 2)

_add_test_target(
    run_test
    Test
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException:java.lang.UnsatisfiedLinkError:java.lang.ClassCastException:java.lang.StringIndexOutOfBoundsException:java.net.MalformedURLException:java.net.ConnectException:java.net.UnknownHostException:java.lang.NumberFormatException:java.io.FileNotFoundException
)
_add_test(run_test 2 1)

_add_test_target(
    run_package
    com.redhat.abrt.test.Test
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException
)
_add_test(run_package 2)

_add_test_target(
    run_jar
    -cp JarTest.jar SimpleTest
    DEPENDS ${JAR_TEST_PATH}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException
)
_add_test(run_jar 2)

_add_test_target(
    run_threads
    MultiThreadTest
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException,executable=threadclass
)
_add_test(run_threads 0)

_add_test_target(
    run_remote
    RemoteTest ${JAR_TEST_PATH}
    PRE rm -f SimpleTest.class
    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException
)
_add_test(run_remote 2)

_add_test_target(
    run_bad_class
    BadClassTest
    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
    AGENT_OPTIONS executable=threadclass
)
_add_test(run_bad_class 2)

# Disabled because JVM catches the ClassNotFoundException in a native method and exits gracefully
#_add_test_target(
#    run_missing_class_test
#    MissingClassTest
#    PRE rm -f MissingClassTest.class
#    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
#    AGENT_OPTIONS executable=threadclass
#)
#_add_test(run_missing_class_test 2)
add_custom_target(
    run_missing_class_test
    COMMAND rm -f MissingClassTest.class && LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=output=run_missing_class.log MissingClassTest || test ! -e run_missing_class.log
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_run_missing_class_test make run_missing_class_test)

_add_test_target(
    run_try_finally
    TryFinallyTest
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException
)
_add_test(run_try_finally 2)


_add_test_target(
    run_inner
    InnerExceptions
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException
)
_add_test(run_inner 2)

_add_test_target(
    run_overriden_equals
    OverridenEqualExceptionTest
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=MaliciousEqualsException
)
#_add_test(run_overriden_eqauls 2)

_add_test_target(
    run_cut_method_namespace
    com/redhat/abrt/test/UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames
    3
    DEPENDS ${TEST_JAVA_TARGETS}
)
_add_test(run_cut_method_namespace 2)

_add_test_target(
    run_cut_exception_namespace
    com/redhat/abrt/test/UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames
    2
    DEPENDS ${TEST_JAVA_TARGETS}
)
_add_test(run_cut_exception_namespace 2)

_add_test_target(
    run_cut_method_class
    com/redhat/abrt/test/UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames
    1
    DEPENDS ${TEST_JAVA_TARGETS}
)
_add_test(run_cut_method_class 2)

_add_test_target(
    run_cut_reason_message
    com/redhat/abrt/test/UnbelievableLongJavaClassNameHavingExtremlyLongAndSenselessMethodNames
    0
    DEPENDS ${TEST_JAVA_TARGETS}
)
_add_test(run_cut_reason_message 2)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^s390x?$")
    set(STRESS_TEST_REPEATS 30)
    set(STRESS_TEST_THREADS 200)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^ppc\(64\)?\(le\)?$")
    set(STRESS_TEST_REPEATS 20)
    set(STRESS_TEST_THREADS 150)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^armv7l$")
    set(STRESS_TEST_REPEATS 10)
    set(STRESS_TEST_THREADS 100)
else()
    set(STRESS_TEST_REPEATS 60)
    set(STRESS_TEST_THREADS 600)
endif()

add_custom_target(
    run_thread_stress
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=caught=java.lang.ArrayIndexOutOfBoundsException,journald=no ThreadStressTest reps=${STRESS_TEST_REPEATS} threads=${STRESS_TEST_THREADS}
    DEPENDS ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_thread_stress  make run_thread_stress)

add_custom_target(
    run_empty_command_line_options
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME} NoException
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_empty_command_line_options make run_empty_command_line_options)

add_custom_target(
    run_no_log_file
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=output=no_file.log NoException && test ! -e no_file.log
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_no_log_file make run_no_log_file)

add_custom_target(
    run_log_file_in_directory
    COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/lid && mkdir ${CMAKE_CURRENT_BINARY_DIR}/lid && LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=output=${CMAKE_CURRENT_BINARY_DIR}/lid Test || test -n `find ${CMAKE_CURRENT_BINARY_DIR}/lid -name "abrt_checker_*.log"`
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_log_file_in_directory make run_log_file_in_directory)

add_custom_target(
    run_default_no_log_file
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME} Test || test -z "`find -name abrt_checker_*.log`"
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_test(test_default_no_log_file make run_default_no_log_file)

_add_test_target(
    run_remote_thread
    RemoteTest ${JAR_TEST_PATH} MultiThreadTest
    PRE rm -f SimpleTest.class ThreadCaughtException.class ThreadUncaughtException.class MultiThreadTest.class
    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException,executable=threadclass
)
_add_test(run_remote_thread 0)

_add_analyze_test(not_reportable_1remote_class)
_add_analyze_test(not_reportable_3remote_classes)
_add_analyze_test(unusable)
_add_analyze_test(unpackaged)

_add_test_target(
    run_three_times
    SimpleTest
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS caught=java.lang.ArrayIndexOutOfBoundsException:java.lang.NullPointerException -agentlib=${AGENT_NAME}=output=/proc/pid/0/java.log -agentlib=${AGENT_NAME}=output=/proc/pid/1/java.log
)
_add_test(run_three_times 2)

_add_test_target(
    run_data_method
    DataMethodTest
    2
    DEPENDS ${TEST_JAVA_TARGETS}
    AGENT_OPTIONS debugmethod=DataMethodTest.debugStringData
)
_add_test(run_data_method 2)

add_custom_target(
    run_jar_relative
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=output=run_jar_relative.log -jar JarTest.jar
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
_add_test(run_jar_relative 2)

add_custom_target(
    run_jar_absolute
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}=output=run_jar_absolute.log -jar ${JAR_TEST_PATH}
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
_add_test(run_jar_absolute 2)

get_directory_property(all_run_targets ALL_RUN_TARGETS)
add_custom_target(run_all DEPENDS ${all_run_targets})
