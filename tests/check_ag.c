/**
 * Copyright (C) 2009 Nokia Corporation.
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
 * Licensed under the terms of Nokia EUSA (see the LICENSE file)
 */

/**
 * @example check_ag.c
 * Shows how to initialize the framework.
 */

#include "libaccounts-glib/ag-account.h"

#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



START_TEST(test_init)
{
    AgManager *manager;

    g_type_init ();
    manager = ag_manager_new ();

    fail_unless (AG_IS_MANAGER (manager),
                 "Failed to initialize the AgManager.");

    g_object_unref(manager);
}
END_TEST

Suite *
ag_suite(void)
{
    Suite *s = suite_create ("accounts-glib");

    /* Core test case */
    TCase * tc_core = tcase_create("Core");
    tcase_add_test (tc_core, test_init);

    suite_add_tcase (s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite * s = ag_suite();
    SRunner * sr = srunner_create(s);

    srunner_set_xml(sr, "/tmp/result.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

