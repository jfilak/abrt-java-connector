find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

include_directories(${PC_LIBREPORT_INCLUDE_DIRS})
include_directories(${PC_ABRT_INCLUDE_DIRS})

pkg_check_modules(PC_JOURNALD libsystemd-journal)

if (PC_JOURNALD_FOUND)
    include_directories(${PC_JOURNALD_INCLUDE_DIRS})
    add_definitions(-DHAVE_SYSTEMD=1)
else()
    add_definitions(-DHAVE_SYSTEMD=0)
endif (PC_JOURNALD_FOUND)

set(AbrtChecker_SRCS configuration.c abrt-checker.c
        jthrowable_circular_buf.c jthread_map.c)

add_definitions(-DVERSION=\"${PROJECT_VERSION}\")

add_library(AbrtChecker SHARED ${AbrtChecker_SRCS})
set_target_properties(
    AbrtChecker
    PROPERTIES
        OUTPUT_NAME abrt-java-connector)

target_link_libraries(AbrtChecker ${PC_LIBREPORT_LIBRARIES})
target_link_libraries(AbrtChecker ${PC_ABRT_LIBRARIES})

if (PC_JOURNALD_FOUND)
    target_link_libraries(AbrtChecker ${PC_JOURNALD_LIBRARIES})
endif (PC_JOURNALD_FOUND)

install(TARGETS AbrtChecker DESTINATION ${JNIAGENTLIB_INSTALL_DIR})
