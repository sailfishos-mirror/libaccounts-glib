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

#define PROVIDER    "dummyprovider"


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

START_TEST(test_object)
{
    AgManager *manager;
    AgAccount *account;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, NULL);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    g_object_unref (account);
    g_object_unref (manager);
}
END_TEST

START_TEST(test_provider)
{
    AgManager *manager;
    AgAccount *account;
    const gchar *provider_name;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    provider_name = ag_account_get_provider_name (account);
    fail_if (strcmp (provider_name, PROVIDER) != 0);

    g_object_unref (account);
    g_object_unref (manager);
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

    TCase * tc_create = tcase_create("Create");
    tcase_add_test (tc_create, test_object);
    tcase_add_test (tc_create, test_provider);

    suite_add_tcase (s, tc_create);

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

