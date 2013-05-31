find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

include(FindPkgConfig)
pkg_check_modules(PC_ABRT REQUIRED abrt)
include_directories(${PC_ABRT_INCLUDE_DIRS})

add_definitions(-D_GNU_SOURCE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99 -pedantic")

set(AbrtChecker_SRCS abrt-checker.c)

add_library(AbrtChecker SHARED ${AbrtChecker_SRCS})
set_property(TARGET AbrtChecker PROPERTY OUTPUT_NAME abrt-java-connector)
target_link_libraries(AbrtChecker ${PC_ABRT_LIBRARIES})

install(TARGETS AbrtChecker DESTINATION ${LIB_INSTALL_DIR})
