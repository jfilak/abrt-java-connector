set(HTTP_DIR "/var/www/html")
set(SERVER_URL "http://localhost")

find_package(Java REQUIRED)

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
        set(CONTENTS ${CONTENTS} ${tmp_name}.class)
    endforeach(tmp_name)

    set(real_path ${CMAKE_CURRENT_BINARY_DIR}/${base_name}.jar)
    set(${output} ${real_path} PARENT_SCOPE)
    add_custom_command(
        OUTPUT ${real_path}
        COMMAND ${Java_JAR_EXECUTABLE} cvf ${base_name}.jar ${CONTENTS}
        DEPENDS ${CLASS_DEPENCIES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

_add_class_target(SimpleTest TEST_JAVA_TARGETS)
_add_class_target(RemoteTest TEST_JAVA_TARGETS SimpleTest)
_add_class_target(MultiThreadTest TEST_JAVA_TARGETS SimpleTest)
_add_class_target(Test TEST_JAVA_TARGETS)
_add_class_target(com/redhat/abrt/test/Test TEST_JAVA_TARGETS)
_add_class_target(BadClassTest TEST_JAVA_TARGETS)
_add_class_target(MissingClass TEST_JAVA_TARGETS)
_add_class_target(MissingClassTest TEST_JAVA_TARGETS MissingClass)

_add_jar_target(JarTest JAR_TEST_PATH SimpleTest)
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
        else()
            set(${current_var} ${${current_var}} ${arg_val})
        endif()
    endforeach(arg_val)

    add_custom_target(
        ${target_name}
        ${pre_command}
        COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED,output=${target_name}.log" ${java_params} -platform.jvmtiSupported true
        DEPENDS AbrtChecker ${depends}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

_add_test_target(
    run
    SimpleTest
    DEPENDS ${TEST_JAVA_TARGETS}
)

_add_test_target(
    run_test
    Test
    DEPENDS ${TEST_JAVA_TARGETS}
)

_add_test_target(
    run_package
    com.redhat.abrt.test.Test
    DEPENDS ${TEST_JAVA_TARGETS}
)

_add_test_target(
    run_jar
    -cp JarTest.jar SimpleTest
    DEPENDS ${JAR_TEST_PATH}
)

_add_test_target(
    run_threads
    MultiThreadTest
    DEPENDS ${TEST_JAVA_TARGETS}
)

add_custom_command(
    OUTPUT ${REMOTE_JAR_PATH}
    COMMAND su -c 'cp ${JAR_TEST_PATH} ${REMOTE_JAR_PATH}' || exit;
    DEPENDS ${JAR_TEST_PATH}
)

_add_test_target(
    run_remote
    RemoteTest ${SERVER_URL}/JarTest.jar
    PRE rm -f SimpleTest.class
    DEPENDS ${TEST_JAVA_TARGETS} ${REMOTE_JAR_PATH}
)

_add_test_target(
    run_bad_class
    BadClassTest
    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
)

_add_test_target(
    run_missing_class_test
    MissingClassTest
    PRE rm -f MissingClassTest.class
    DEPENDS ${TEST_JAVA_TARGETS} ${JAR_TEST_PATH}
)
