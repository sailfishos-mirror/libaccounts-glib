/**
 * Copyright (C) 2009 Nokia Corporation.
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
 * Licensed under the terms of Nokia EUSA (see the LICENSE file)
 */

/**
 * @example check_ag.c
 * Shows how to initialize the framework.
 */

#include "libaccounts-glib/ag-manager.h"
#include "libaccounts-glib/ag-account.h"
#include "libaccounts-glib/ag-errors.h"
#include "libaccounts-glib/ag-internals.h"

#include <glib.h>
#include <glib/gstdio.h>
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
static AgService *service = NULL;
static gboolean data_stored = FALSE;

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
    if (service)
    {
        ag_service_unref (service);
        service = NULL;
    }

    if (main_loop)
    {
        g_main_loop_quit (main_loop);
        g_main_loop_unref (main_loop);
        main_loop = NULL;
    }

    data_stored = FALSE;
}

START_TEST(test_init)
{
    g_type_init ();
    manager = ag_manager_new ();

    fail_unless (AG_IS_MANAGER (manager),
                 "Failed to initialize the AgManager.");

    end_test ();
}
END_TEST

START_TEST(test_object)
{
    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, NULL);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    end_test ();
}
END_TEST

START_TEST(test_provider)
{
    const gchar *provider_name;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    provider_name = ag_account_get_provider_name (account);
    fail_if (strcmp (provider_name, PROVIDER) != 0);

    end_test ();
}
END_TEST

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    const gchar *string = user_data;

    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");

    end_test ();
}

START_TEST(test_store)
{
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
    else
        end_test ();
}
END_TEST

void account_store_locked_cb (AgAccount *account, const GError *error,
                              gpointer user_data)
{
    const gchar *string = user_data;

    g_debug ("%s called", G_STRFUNC);
    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");

    fail_unless (lock_released, "Data stored while DB locked!");

    end_test ();
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
    fail_unless (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
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
    g_timeout_add (10, (GSourceFunc)unref_account, NULL);
    g_timeout_add (20, (GSourceFunc)release_lock_unref, db);
    fail_unless (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
}
END_TEST

void account_store_now_cb (AgAccount *account, const GError *error,
                           gpointer user_data)
{
    const gchar *string = user_data;

    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (strcmp (string, TEST_STRING) == 0, "Got wrong string");

    data_stored = TRUE;
}

START_TEST(test_service)
{
    GValue value = { 0 };
    AgService *service2;
    AgAccountId account_id;
    const gchar *provider_name, *service_type, *service_name;
    const gchar *description = "This is really a beautiful account";
    const gchar *username = "me@myhome.com";
    const gint interval = 30;
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    AgSettingSource source;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, description);
    ag_account_set_value (account, "description", &value);
    g_value_unset (&value);

    service = ag_manager_get_service (manager, "MyUnexistingService");
    fail_unless (service == NULL);

    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL);

    service_type = ag_service_get_service_type (service);
    fail_unless (strcmp (service_type, "e-mail") == 0,
                 "Wrong service type: %s", service_type);

    service_name = ag_service_get_name (service);
    fail_unless (strcmp (service_name, "MyService") == 0,
                 "Wrong service name: %s", service_name);

    service_name = ag_service_get_display_name (service);
    fail_unless (strcmp (service_name, "My Service") == 0,
                 "Wrong service display name: %s", service_name);

    ag_account_set_enabled (account, FALSE);
    ag_account_set_display_name (account, display_name);

    ag_account_select_service (account, service);

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "parameters/port", &value);
    fail_unless (source == AG_SETTING_SOURCE_PROFILE,
                 "Cannot get port from profile");
    fail_unless (g_value_get_int (&value) == 5223,
                 "Wrong port number: %d", g_value_get_int (&value));
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, username);
    ag_account_set_value (account, "username", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, check_automatically);
    ag_account_set_value (account, "check_automatically", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, interval);
    ag_account_set_value (account, "interval", &value);
    g_value_unset (&value);

    service2 = ag_manager_get_service (manager, "OtherService");
    ag_account_select_service (account, service2);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "Wednesday");
    ag_account_set_value (account, "day", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_set_value (account, "ForReal", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    g_debug ("Service id: %d", service->id);
    g_debug ("Service2 id: %d", service2->id);
    g_debug ("Account id: %d", account->id);
    account_id = account->id;

    ag_service_unref (service2);
    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();
    account = ag_manager_get_account (manager, account_id);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Couldn't load account %u", account_id);

    provider_name = ag_account_get_provider_name (account);
    fail_unless (strcmp (provider_name, PROVIDER) == 0,
                 "Got provider %s, expecting %s", provider_name, PROVIDER);

    /* check that the values are retained */
    fail_unless (ag_account_get_enabled (account) == FALSE,
                 "Account enabled!");
    fail_unless (strcmp (ag_account_get_display_name (account),
                         display_name) == 0,
                 "Display name not retained!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "description", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (strcmp(g_value_get_string (&value), description) == 0,
                 "Wrong value");
    g_value_unset (&value);

    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "username", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (strcmp(g_value_get_string (&value), username) == 0,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    source = ag_account_get_value (account, "check_automatically", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_boolean (&value) == check_automatically,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "interval", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_int (&value) == interval, "Wrong value");
    g_value_unset (&value);

    /* check also value conversion */
    g_value_init (&value, G_TYPE_CHAR);
    source = ag_account_get_value (account, "interval", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_char (&value) == interval, "Wrong value");
    g_value_unset (&value);

    /* change a value */
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "Friday");
    ag_account_set_value (account, "day", &value);
    g_value_unset (&value);

    /* change global enabledness */
    ag_account_select_service (account, NULL);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (ag_account_get_enabled (account) == TRUE,
                 "Account still disabled!");
    end_test ();
}
END_TEST

static void
set_boolean_variable (gboolean *flag)
{
    *flag = TRUE;
}

START_TEST(test_signals)
{
    const gchar *display_name = "My lovely account";
    gboolean enabled_called = FALSE;
    gboolean display_name_called = FALSE;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_signal_connect_swapped (account, "enabled",
                              G_CALLBACK (set_boolean_variable),
                              &enabled_called);
    g_signal_connect_swapped (account, "display-name-changed",
                              G_CALLBACK (set_boolean_variable),
                              &display_name_called);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);


    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (enabled_called, "Enabled signal not emitted!");
    fail_unless (display_name_called, "DisplayName signal not emitted!");

    end_test ();
}
END_TEST

START_TEST(test_list)
{
    const gchar *display_name = "New account";
    const gchar *service_name = "OtherService";
    const gchar *service_type;
    GValue value = { 0 };
    AgSettingSource source;
    GList *list;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");

    list = ag_manager_list (manager);
    fail_unless (list != NULL, "Empty list");
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                 "Created account not found in list");
    g_list_free (list);

    /* check that it doesn't support the service type provided by MyService */
    service = ag_manager_get_service (manager, service_name);
    service_type = ag_service_get_service_type (service);
    fail_unless (service_type != NULL, "Service %s has no type", service_name);

    list = ag_manager_list_by_service_type (manager, service_type);
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) == NULL,
                 "New account supports %s service type, but shouldn't",
                 service_type);
    g_list_free (list);

    /* Add the service to the account */
    fail_unless (service != NULL);

    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "parameters/port", &value);
    fail_unless (source == AG_SETTING_SOURCE_PROFILE,
                 "Cannot get port from profile");
    fail_unless (g_value_get_int (&value) == 5223,
                 "Wrong port number: %d", g_value_get_int (&value));
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* check that the service is now there */
    list = ag_manager_list_by_service_type (manager, service_type);
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                 "New account doesn't supports %s service type, but should",
                 service_type);
    g_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_settings_iter)
{
    const gchar *keys[] = {
        "param/address",
        "weight",
        "param/city",
        "age",
        "param/country",
        NULL,
    };
    const gchar *values[] = {
        "Helsinginkatu",
        "110",
        "Helsinki",
        "90",
        "Suomi",
        NULL,
    };
    const gchar *service_name = "OtherService";
    GValue value = { 0 };
    AgAccountSettingIter iter;
    const gchar *key;
    const GValue *val;
    gint i, n_values, n_read;
    const gint new_port_value = 432412;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ag_account_set_enabled (account, TRUE);

    for (i = 0; keys[i] != NULL; i++)
    {
        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, values[i]);
        ag_account_set_value (account, keys[i], &value);
        g_value_unset (&value);
    }
    n_values = i;

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");

    /* iterate the settings */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        for (i = 0; keys[i] != NULL; i++)
        {
            if (strcmp (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                fail_unless (strcmp (values[i], text) == 0,
                             "Got value %s for key %s, expecting %s",
                             text, key, values[i]);
                break;
            }
        }

        fail_unless (found, "Unknown setting %s", key);

        n_read++;
    }

    fail_unless (n_read == n_values,
                 "Not all settings were retrieved (%d out of %d)",
                 n_read, n_values);

    /* iterate settings with prefix */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "param/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        fail_unless (strncmp (key, "param/", 6) == 0,
                     "Got key %s, wrong prefix", key);
        for (i = 0; keys[i] != NULL; i++)
        {
            if (strcmp (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                fail_unless (strcmp (values[i], text) == 0,
                             "Got value %s for key %s, expecting %s",
                             text, key, values[i]);
                break;
            }
        }

        fail_unless (found, "Unknown setting %s", key);

        n_read++;
    }

    fail_unless (n_read == 3, "Not all settings were retrieved");

    /* iterate template default settings */
    service = ag_manager_get_service (manager, service_name);
    ag_account_select_service (account, service);
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        g_debug ("Got key %s of type %s", key, G_VALUE_TYPE_NAME (val));

        n_read++;
    }
    fail_unless (n_read == 4, "Not all settings were retrieved");

    /* Add a setting that is also on the template, to check if it will
     * override the one on the template */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, new_port_value);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    /* Add a setting */
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "How's life?");
    ag_account_set_value (account, "parameters/message", &value);
    g_value_unset (&value);

    /* save */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* enumerate the parameters */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "parameters/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        fail_unless (g_str_has_prefix (key, "parameters/"),
                     "Got key %s, wrong prefix", key);
        g_debug ("Got key %s of type %s", key, G_VALUE_TYPE_NAME (val));
        if (strcmp (key, "parameters/port") == 0)
        {
            gint port;

            port = g_value_get_int (val);
            fail_unless (port == new_port_value,
                         "Got value %d for key %s, expecting %d",
                         port, key, new_port_value);
        }

        n_read++;
    }

    fail_unless (n_read == 5, "Not all settings were retrieved");


    end_test ();
}
END_TEST

START_TEST(test_list_services)
{
    GList *services, *list;
    gint n_services;
    AgService *service;
    const gchar *name;

    g_type_init ();

    manager = ag_manager_new ();

    /* get all services */
    services = ag_manager_list_services (manager);

    n_services = g_list_length (services);
    fail_unless (n_services == 2, "Got %d services, expecting 2", n_services);

    for (list = services; list != NULL; list = list->next)
    {
        service = list->data;

        name = ag_service_get_name (service);
        g_debug ("Service name: %s", name);
        fail_unless (strcmp (name, "MyService") == 0 ||
                     strcmp (name, "OtherService") == 0,
                     "Got unexpected service `%s'", name);
    }
    ag_service_list_free (services);

    /* get services by type */
    services = ag_manager_list_services_by_type (manager, "sharing");

    n_services = g_list_length (services);
    fail_unless (n_services == 1, "Got %d services, expecting 1", n_services);

    list = services;
    service = list->data;
    name = ag_service_get_name (service);
    fail_unless (strcmp (name, "OtherService") == 0,
                 "Got unexpected service `%s'", name);
    ag_service_list_free (services);

    end_test ();
}
END_TEST

START_TEST(test_delete)
{
    AgAccountId id;
    gboolean enabled_called, deleted_called;

    g_type_init ();

    manager = ag_manager_new ();

    /* create an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");
    id = account->id;

    /* monitor the account status */
    g_signal_connect_swapped (account, "enabled",
                              G_CALLBACK (set_boolean_variable),
                              &enabled_called);
    g_signal_connect_swapped (account, "deleted",
                              G_CALLBACK (set_boolean_variable),
                              &deleted_called);
    enabled_called = deleted_called = FALSE;

    /* delete it */
    ag_account_delete (account);

    /* until ag_account_store() is called, the signals should not have been
     * emitted */
    fail_unless (enabled_called == FALSE, "Accound disabled too early!");
    fail_unless (deleted_called == FALSE, "Accound deleted too early!");

    /* really delete the account */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* check that the signals are emitted */
    fail_unless (enabled_called, "Accound enabled signal not emitted");
    fail_unless (deleted_called, "Accound deleted signal not emitted");

    g_object_unref (account);

    /* load the account again: this must fail */
    account = ag_manager_get_account (manager, id);
    fail_unless (account == NULL, "The account still exists");

    end_test ();
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
    tcase_add_test (tc_create, test_service);
    tcase_add_test (tc_create, test_signals);
    tcase_add_test (tc_create, test_list);
    tcase_add_test (tc_create, test_settings_iter);
    tcase_add_test (tc_create, test_list_services);
    tcase_add_test (tc_create, test_delete);

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
    g_unlink (db_filename);

    srunner_set_xml(sr, "/tmp/result.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free (sr);

    g_free (db_filename);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

