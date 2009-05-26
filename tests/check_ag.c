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
#include "libaccounts-glib/ag-errors.h"

#include <glib.h>
#include <check.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PROVIDER    "dummyprovider"
#define TEST_STRING "Hey dude!"

static gchar *db_filename;
static GMainLoop *main_loop = NULL;
gboolean lock_released = FALSE;
static AgAccount *account = NULL;
static AgManager *manager = NULL;

static void
end_test ()
{
    if (account)
    {
        g_object_unref (account);
        account = NULL;
    }
    if (manager)
    {
        g_object_unref (manager);
        manager = NULL;
    }
    if (main_loop)
    {
        g_main_loop_quit (main_loop);
        g_main_loop_unref (main_loop);
        main_loop = NULL;
    }
}

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

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    AgManager *manager;
    const gchar *string = user_data;

    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");

    manager = ag_account_get_manager (account);
    g_object_unref (account);
    g_object_unref (manager);

    g_main_loop_quit (main_loop);
    g_main_loop_unref (main_loop);
    main_loop = NULL;
}

START_TEST(test_store)
{
    AgManager *manager;
    AgAccount *account;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_cb, TEST_STRING);
    if (main_loop)
    {
        g_debug ("Running loop");
        g_main_loop_run (main_loop);
    }
}
END_TEST

void account_store_locked_cb (AgAccount *account, const GError *error,
                              gpointer user_data)
{
    AgManager *manager;
    const gchar *string = user_data;

    g_debug ("%s called", G_STRFUNC);
    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");

    fail_unless (lock_released, "Data stored while DB locked!");

    manager = ag_account_get_manager (account);
    g_object_unref (account);
    g_object_unref (manager);

    g_main_loop_quit (main_loop);
    g_main_loop_unref (main_loop);
    main_loop = NULL;
}

gboolean
release_lock (sqlite3 *db)
{
    g_debug ("releasing lock");
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);
    lock_released = TRUE;
    return FALSE;
}

START_TEST(test_store_locked)
{
    AgManager *manager;
    AgAccount *account;
    sqlite3 *db;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_locked_cb, TEST_STRING);
    g_timeout_add (100, (GSourceFunc)release_lock, db);
    if (main_loop)
    {
        g_debug ("Running loop");
        g_main_loop_run (main_loop);
    }
}
END_TEST

static void
account_store_locked_unref_cb (AgAccount *account, const GError *error,
                               gpointer user_data)
{
    const gchar *string = user_data;

    g_debug ("%s called", G_STRFUNC);
    fail_unless (error != NULL, "Account disposed but no error set!");
    fail_unless (error->code == AG_ERROR_DISPOSED,
                 "Got a different error code");
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");
}

static void
release_lock_unref (sqlite3 *db)
{
    g_debug ("releasing lock");
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);

    end_test ();
}

static gboolean
unref_account (gpointer user_data)
{
    g_debug ("Unreferencing account %p", account);
    fail_unless (AG_IS_ACCOUNT (account), "Account disposed?");
    g_object_unref (account);
    account = NULL;
    return FALSE;
}

START_TEST(test_store_locked_unref)
{
    sqlite3 *db;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_locked_unref_cb, TEST_STRING);
    g_timeout_add (10, (GSourceFunc)unref_account, account);
    g_timeout_add (20, (GSourceFunc)release_lock_unref, db);
    if (main_loop)
    {
        g_debug ("Running loop");
        g_main_loop_run (main_loop);
    }
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
    tcase_add_test (tc_create, test_store);
    tcase_add_test (tc_create, test_store_locked);
    tcase_add_test (tc_create, test_store_locked_unref);

    suite_add_tcase (s, tc_create);

    return s;
}

int main(void)
{
    int number_failed;
    Suite * s = ag_suite();
    SRunner * sr = srunner_create(s);

    db_filename = g_build_filename (g_getenv ("ACCOUNTS"), "accounts.db",
                                    NULL);

    srunner_set_xml(sr, "/tmp/result.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free (sr);

    g_free (db_filename);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

