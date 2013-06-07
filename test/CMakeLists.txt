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
# Help:
#
#   to run test with errors reporting to ABRT issue a command like this:
#
#   $ make run ABRT_ENABLED="on"
#

add_custom_target(
    run
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" SimpleTest -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_test
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" Test -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_package
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" com.redhat.abrt.test.Test -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_jar
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" -cp JarTest.jar SimpleTest -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${JAR_TEST_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_threads
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" MultiThreadTest -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${TEST_JAVA_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
    OUTPUT ${REMOTE_JAR_PATH}
    COMMAND su -c 'cp ${JAR_TEST_PATH} ${REMOTE_JAR_PATH}' || exit;
    DEPENDS ${JAR_TEST_PATH}
)

add_custom_target(
    run_remote
    rm -rf SimpleTest.class
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" RemoteTest ${SERVER_URL}/JarTest.jar -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${REMOTE_JAR_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_bad_class
    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" BadClassTest -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${JAR_TEST_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(
    run_missing_class_test
    rm -f MissingClass.class
    COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src ${Java_JAVA_EXECUTABLE} -agentlib:${AGENT_NAME}="abrt=$$ABRT_ENABLED" MissingClassTest -platform.jvmtiSupported true
    DEPENDS AbrtChecker ${JAR_TEST_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

