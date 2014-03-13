#include "abrt-checker.h"
#include "internal_libabrt.h"

#include <stdlib.h>
#include <check.h>

void assert_str_vector_eq(const char **expected, const char **tested)
{
    while (*expected && *tested)
    {
        ck_assert_str_eq(*expected, *tested);

        ++expected;
        ++tested;
    }

    ck_assert(*expected == *tested);
}

void assert_conf_populated(T_configuration *conf)
{
    ck_assert_int_eq((conf->reportErrosTo & ED_ABRT), ED_ABRT);
    ck_assert_int_eq((conf->reportErrosTo & ED_SYSLOG), ED_SYSLOG);
    ck_assert_int_eq((conf->reportErrosTo & ED_JOURNALD), 0);

    ck_assert_int_eq((conf->executableFlags & ABRT_EXECUTABLE_MAIN), 0);
    ck_assert_int_eq((conf->executableFlags & ABRT_EXECUTABLE_THREAD), ABRT_EXECUTABLE_THREAD);

    ck_assert_str_eq(conf->outputFileName, "test.log");

    ck_assert_str_eq(conf->configurationFileName, "java.conf");

    ck_assert(conf->reportedCaughExceptionTypes != NULL);
    const char *caughtTypes[] = { "n.s.Ex1", "n.s.Ex2", "n.s.Ex3", NULL };
    assert_str_vector_eq((const char **)caughtTypes, (const char **)conf->reportedCaughExceptionTypes);

    ck_assert(conf->fqdnDebugMethods != NULL);
    const char *debugMethods[] = { "n.s.cls.M1", "n.s.cls2.M2", "n.s.cls3.M3", NULL };
    assert_str_vector_eq((const char **)debugMethods, (const char **)conf->fqdnDebugMethods);
}

START_TEST(test_config_file_all_entries_populated)
{
    T_configuration conf;
    configuration_initialize(&conf);

    mark_point();
    parse_configuration_file(&conf, CONFIG_FILE_ALL_ENTRIES_POPULATED".conf");

    mark_point();
    assert_conf_populated(&conf);

    configuration_destroy(&conf);
}
END_TEST

START_TEST(test_command_line_conf_all_entries_populated)
{
    T_configuration conf;
    configuration_initialize(&conf);

    char *opts = strdup(
            "abrt=on,syslog=on,journald=off,executable=threadclass,output=test.log,"
            "caught=n.s.Ex1:n.s.Ex2:n.s.Ex3,debugmethod=n.s.cls.M1:n.s.cls2.M2:n.s.cls3.M3");

    ck_assert_msg(NULL != opts, "Out of memory");

    mark_point();
    parse_commandline_options(&conf, opts);

    mark_point();
    assert_conf_populated(&conf);

    configuration_destroy(&conf);
}
END_TEST

START_TEST(test_conf_file_no_overwrite)
{
    T_configuration conf;
    configuration_initialize(&conf);

    char *opts = strdup(
            "abrt=off,syslog=off,journald=on,executable=mainclass,output=,"
            "conffile=,caught=,debugmethod=");

    ck_assert_msg(NULL != opts, "Out of memory");

    mark_point();
    parse_commandline_options(&conf, opts);

    mark_point();
    parse_configuration_file(&conf, CONFIG_FILE_ALL_ENTRIES_POPULATED".conf");

    ck_assert_int_eq((conf.reportErrosTo & ED_ABRT), 0);
    ck_assert_int_eq((conf.reportErrosTo & ED_SYSLOG), 0);
    ck_assert_int_eq((conf.reportErrosTo & ED_JOURNALD), ED_JOURNALD);

    ck_assert_int_eq((conf.executableFlags & ABRT_EXECUTABLE_MAIN),  ABRT_EXECUTABLE_MAIN);
    ck_assert_int_eq((conf.executableFlags & ABRT_EXECUTABLE_THREAD), 0);

    ck_assert(conf.outputFileName == DISABLED_LOG_OUTPUT);

    ck_assert(NULL == conf.configurationFileName);

    ck_assert(NULL == conf.reportedCaughExceptionTypes);

    ck_assert(NULL == conf.fqdnDebugMethods);

    configuration_destroy(&conf);
}
END_TEST

Suite *abrt_checker_suite(void)
{
    Suite *s = suite_create ("abrt-checker");

    /* Configuration test case */
    TCase *tc_configuration = tcase_create("Configuration");
    tcase_add_test(tc_configuration, test_config_file_all_entries_populated);
    tcase_add_test(tc_configuration, test_command_line_conf_all_entries_populated);
    tcase_add_test(tc_configuration, test_conf_file_no_overwrite);
    suite_add_tcase(s, tc_configuration);

    return s;
}


int main(void)
{
    int number_failed;
    Suite *s = abrt_checker_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
