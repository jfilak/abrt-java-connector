find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

include(FindPkgConfig)
pkg_check_modules(PC_ABRT REQUIRED abrt)
include_directories(${PC_ABRT_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -ansi")

set(AbrtChecker_SRCS abrt-checker.c)

add_library(AbrtChecker SHARED ${AbrtChecker_SRCS})
target_link_libraries(AbrtChecker ${PC_ABRT_LIBRARIES})

install(TARGETS AbrtChecker DESTINATION ${LIB_INSTALL_DIR})
