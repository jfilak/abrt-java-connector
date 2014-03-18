pkg_check_modules(PC_CHECK REQUIRED check)

add_definitions(-DCONFIG_FILE_ALL_ENTRIES_POPULATED="${CMAKE_CURRENT_SOURCE_DIR}/config_file_all_entries_populated")

include_directories(${PC_ABRT_INCLUDE_DIRS})
include_directories(${PC_CHECK_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/src")

add_executable(testsuite check_abrt_java_connector.c)
target_link_libraries(testsuite ${PC_CHECK_LIBRARIES})
target_link_libraries(testsuite AbrtChecker)

add_test(unit_tests ./testsuite)
