find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

include(FindPkgConfig)
pkg_check_modules(PC_ABRT REQUIRED libreport)
include_directories(${PC_ABRT_INCLUDE_DIRS})

add_definitions(-D_GNU_SOURCE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99 -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DVERBOSE")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DSILENT")

set(AbrtChecker_SRCS abrt-checker.c jthrowable_circular_buf.c jthread_map.c)

add_library(AbrtChecker SHARED ${AbrtChecker_SRCS})
set_target_properties(
    AbrtChecker
    PROPERTIES
        OUTPUT_NAME abrt-java-connector
        SOVERSION 0)

target_link_libraries(AbrtChecker ${PC_ABRT_LIBRARIES})

install(TARGETS AbrtChecker DESTINATION ${LIB_INSTALL_DIR})
