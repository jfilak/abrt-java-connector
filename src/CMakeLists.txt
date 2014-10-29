find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

pkg_check_modules(PC_JOURNALD REQUIRED libsystemd-journal)
include_directories(${PC_LIBREPORT_INCLUDE_DIRS})
include_directories(${PC_JOURNALD_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99 -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DVERBOSE")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DSILENT")

set(AbrtChecker_SRCS abrt-checker.c jthrowable_circular_buf.c jthread_map.c)

add_library(AbrtChecker SHARED ${AbrtChecker_SRCS})
set_target_properties(
    AbrtChecker
    PROPERTIES
        OUTPUT_NAME abrt-java-connector)

target_link_libraries(AbrtChecker ${PC_LIBREPORT_LIBRARIES})
target_link_libraries(AbrtChecker ${PC_JOURNALD_LIBRARIES})

install(TARGETS AbrtChecker DESTINATION ${JNIAGENTLIB_INSTALL_DIR})
