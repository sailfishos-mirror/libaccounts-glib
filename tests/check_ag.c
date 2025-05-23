/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012-2016 Canonical Ltd.
 * Copyright (C) 2012 Intel Corporation.
 * Copyright (C) 2020 Open Mobile Platform LLC.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * @example check_ag.c
 * Shows how to initialize the framework.
 */

#define AG_DISABLE_DEPRECATION_WARNINGS

#define MAX_SQLITE_BUSY_LOOP_TIME 5
#define MAX_SQLITE_BUSY_LOOP_TIME_MS (MAX_SQLITE_BUSY_LOOP_TIME * 1000)

#include "test-manager.h"

#include <libaccounts-glib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <check.h>
#include <sched.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PROVIDER    "dummyprovider"
#define TEST_STRING "Hey dude!"
#define TEST_SERVICE_VALUE  "calendar"

static gchar *db_filename;
static GMainLoop *main_loop = NULL;
gboolean lock_released = FALSE;
static AgAccount *account = NULL;
static AgManager *manager = NULL;
static AgService *service = NULL;
static gboolean data_stored = FALSE;
static guint source_id = 0;
static guint idle_finish = 0;

typedef struct {
    gboolean called;
    gchar *service;
    gboolean enabled_check;
} EnabledCbData;

typedef struct {
    gint called_count;
    gboolean enabled;
} AccountServiceEnabledCbData;

typedef struct {
    gboolean called;
    guint account_id;
    gboolean created;
    gboolean deleted;
    gchar *provider;
    GVariant *settings;
} StoreCbData;

static void
store_cb_data_unset (StoreCbData *data)
{
    g_free (data->provider);
    g_variant_unref (data->settings);
    memset (data, 0, sizeof (*data));
}

static void
set_read_only()
{
    gchar *filename;

    /* This is just to ensure that the DB exists */
    manager = ag_manager_new ();
    g_object_unref (manager);

    chmod (db_filename, S_IRUSR | S_IRGRP | S_IROTH);

    filename = g_strconcat (db_filename, "-shm", NULL);
    chmod (filename, S_IRUSR | S_IRGRP | S_IROTH);
    g_free (filename);

    filename = g_strconcat (db_filename, "-wal", NULL);
    unlink (filename);
    g_free (filename);
}

static void
delete_db()
{
    gchar *filename;

    unlink (db_filename);

    filename = g_strconcat (db_filename, "-shm", NULL);
    unlink (filename);
    g_free (filename);

    filename = g_strconcat (db_filename, "-wal", NULL);
    unlink (filename);
    g_free (filename);
}

static void
on_enabled (AgAccount *account, const gchar *service, gboolean enabled,
            EnabledCbData *ecd)
{
    ecd->called = TRUE;
    ecd->service = g_strdup (service);
    ecd->enabled_check = (ag_account_get_enabled (account) == enabled);
}

static gboolean
quit_loop (gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit (loop);
    return FALSE;
}

static void
run_main_loop_for_n_seconds(guint seconds)
{
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_timeout_add_seconds (seconds, quit_loop, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
}

static gboolean
test_strv_equal (const gchar **s1, const gchar **s2)
{
    gint i;

    if (s1 == NULL) return s2 == NULL;
    if (s1 != NULL && s2 == NULL) return FALSE;

    for (i = 0; s1[i] != NULL; i++)
        if (strcmp(s1[i], s2[i]) != 0) {
            g_debug ("s1: %s, s2: %s", s1[i], s2[i]);
            return FALSE;
        }
    if (s2[i] != NULL) return FALSE;

    return TRUE;
}

static guint
time_diff(struct timespec *start_time, struct timespec *end_time)
{
    struct timespec diff_time;
    diff_time.tv_sec = end_time->tv_sec - start_time->tv_sec;
    diff_time.tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
    return diff_time.tv_sec * 1000 + diff_time.tv_nsec / 1000000;
}

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
    manager = ag_manager_new ();

    ck_assert_msg (AG_IS_MANAGER (manager),
                   "Failed to initialize the AgManager.");

    end_test ();
}
END_TEST

START_TEST(test_timeout_properties)
{
    gboolean abort_on_db_timeout;
    guint db_timeout;

    manager = ag_manager_new ();
    ck_assert (AG_IS_MANAGER (manager));

    g_object_get (manager,
                  "db-timeout", &db_timeout,
                  "abort-on-db-timeout", &abort_on_db_timeout,
                  NULL);

    ck_assert (!abort_on_db_timeout);
    ck_assert (!ag_manager_get_abort_on_db_timeout (manager));
    ck_assert_uint_eq (db_timeout, ag_manager_get_db_timeout (manager));

    g_object_set (manager,
                  "db-timeout", 120,
                  "abort_on_db_timeout", TRUE,
                  NULL);
    ck_assert (ag_manager_get_abort_on_db_timeout (manager));
    ck_assert_uint_eq (ag_manager_get_db_timeout (manager), 120);

    end_test ();
}
END_TEST

START_TEST(test_object)
{
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, NULL);
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");

    end_test ();
}
END_TEST

START_TEST(test_read_only)
{
    GError *error = NULL;
    gboolean ok;

    set_read_only ();

    manager = ag_manager_new ();
    ck_assert (manager != NULL);

    /* create an account, and expect a failure */
    account = ag_manager_create_account (manager, "bisbone");
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");

    ok = ag_account_store_blocking (account, &error);
    ck_assert (!ok);
    ck_assert (error->code == AG_ACCOUNTS_ERROR_READONLY);
    g_debug ("Error message: %s", error->message);
    g_error_free (error);

    /* delete the DB */
    g_object_unref (account);
    account = NULL;
    g_object_unref (manager);
    manager = NULL;

    delete_db ();

    g_debug("Ending read-only test");

    end_test ();
}
END_TEST

START_TEST(test_provider)
{
    const gchar *provider_name, *display_name;
    const gchar *description;
    const gchar *domains;
    const gchar *plugin_name;
    AgProvider *provider;
    GList *providers, *list, *tags;
    gboolean single_account;
    gboolean found;

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");

    provider_name = ag_account_get_provider_name (account);
    fail_if (g_strcmp0 (provider_name, PROVIDER) != 0);

    /* Test provider XML file loading */
    provider = ag_manager_get_provider (manager, "MyProvider");
    ck_assert (provider != NULL);

    ck_assert_str_eq (ag_provider_get_name (provider), "MyProvider");
    ck_assert_str_eq (ag_provider_get_i18n_domain (provider),
                      "provider_i18n");
    ck_assert_str_eq (ag_provider_get_icon_name (provider),
                      "general_myprovider");

    display_name = ag_provider_get_display_name (provider);
    ck_assert (g_strcmp0 (display_name, "My Provider") == 0);

    description = ag_provider_get_description (provider);
    ck_assert (g_strcmp0 (description, "My Provider Description") == 0);

    single_account = ag_provider_get_single_account (provider);
    ck_assert (single_account);

    tags = ag_provider_get_tags (provider);
    ck_assert (tags != NULL);
    for (list = tags; list != NULL; list = list->next)
    {
        const gchar *tag = list->data;
        g_debug(" Provider tag: %s", tag);
        ck_assert_msg (g_strcmp0 (tag, "user-group:mygroup") == 0,
                       "Wrong service tag: %s", tag);
    }
    g_list_free (tags);

    /* The next couple of lines serve only to add coverage for
     * ag_provider_ref() */
    ag_provider_ref (provider);
    ag_provider_unref (provider);

    ag_provider_unref (provider);

    provider = ag_manager_get_provider (manager, "maemo");
    ck_assert (provider != NULL);

    single_account = ag_provider_get_single_account (provider);
    ck_assert (!single_account);

    ag_provider_unref (provider);

    /* Test provider enumeration */
    providers = ag_manager_list_providers (manager);
    ck_assert (providers != NULL);
    ck_assert (g_list_length (providers) == 2);

    found = FALSE;
    for (list = providers; list != NULL; list = list->next)
    {
        provider = list->data;
        display_name = ag_provider_get_display_name (provider);
        if (g_strcmp0 (display_name, "My Provider") != 0) continue;

        found = TRUE;
        domains = ag_provider_get_domains_regex (provider);
        ck_assert (g_strcmp0 (domains, ".*provider\\.com") == 0);

        ck_assert (ag_provider_match_domain (provider, "www.provider.com"));

        plugin_name = ag_provider_get_plugin_name (provider);
        ck_assert (g_strcmp0 (plugin_name, "oauth2") == 0);
    }

    ck_assert (found);

    ag_provider_list_free (providers);

    end_test ();
}
END_TEST

START_TEST(test_provider_settings)
{
    AgSettingSource source;
    GVariant *variant;

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, "MyProvider");
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");

    /* Test provider default settings */
    source = AG_SETTING_SOURCE_NONE;
    variant = ag_account_get_variant (account, "login/server", &source);
    ck_assert (source == AG_SETTING_SOURCE_PROFILE);
    ck_assert (variant != NULL);
    ck_assert (g_strcmp0 (g_variant_get_string (variant, NULL),
                            "login.example.com") == 0);

    source = AG_SETTING_SOURCE_NONE;
    variant = ag_account_get_variant (account, "login/remember-me", &source);
    ck_assert (source == AG_SETTING_SOURCE_PROFILE);
    ck_assert (variant != NULL);
    ck_assert (g_variant_get_boolean (variant) == TRUE);

    end_test ();
}
END_TEST

START_TEST(test_provider_directories)
{
    AgProvider *provider;
    gchar *ag_providers_env;
    gchar *xdg_data_home_env;

    /* Unset the AG_PROVIDERS environment variable, just for this test, as
     * that disables the fallback mechanism which we now want to test. */
    ag_providers_env = g_strdup (g_getenv ("AG_PROVIDERS"));
    g_unsetenv ("AG_PROVIDERS");
    /* Disable also XDG_DATA_HOME, but reuse its value for XDG_DATA_DIRS
     * which is where the fallback mechanism is implemented. */
    xdg_data_home_env = g_strdup (g_getenv ("XDG_DATA_HOME"));
    g_unsetenv ("XDG_DATA_HOME");
    g_setenv ("XDG_DATA_DIRS", xdg_data_home_env, TRUE);

    /* check that the expected MyProvider file is loaded */
    g_unsetenv ("XDG_CURRENT_DESKTOP");
    manager = ag_manager_new ();

    provider = ag_manager_get_provider (manager, "MyProvider");
    ck_assert (provider != NULL);
    ck_assert_str_eq (ag_provider_get_name (provider), "MyProvider");
    ck_assert_str_eq (ag_provider_get_display_name (provider), "My Provider");

    ag_provider_unref (provider);
    g_object_unref (manager);

    /* Now check a desktop-specific override */
    g_setenv ("XDG_CURRENT_DESKTOP", "Fake-OS", TRUE);
    manager = ag_manager_new ();

    provider = ag_manager_get_provider (manager, "MyProvider");
    ck_assert (provider != NULL);
    ck_assert_str_eq (ag_provider_get_name (provider), "MyProvider");
    ck_assert_str_eq (ag_provider_get_display_name (provider), "FakeOs Provider");

    ag_provider_unref (provider);
    g_object_unref (manager);

    g_unsetenv ("XDG_DATA_DIRS");
    g_setenv ("XDG_DATA_HOME", xdg_data_home_env, TRUE);
    g_free (xdg_data_home_env);
    g_setenv ("AG_PROVIDERS", ag_providers_env, TRUE);
    g_free (ag_providers_env);
    manager = NULL;
    end_test ();
}
END_TEST

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    const gchar *string = user_data;

    ck_assert_msg (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        ck_abort_msg("Got error: %s", error->message);
    ck_assert_msg (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    end_test ();
}

START_TEST(test_store)
{
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
    ck_assert_msg (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        ck_abort_msg("Got error: %s", error->message);
    ck_assert_msg (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    ck_assert_msg (lock_released, "Data stored while DB locked!");

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

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_locked_cb, TEST_STRING);
    g_timeout_add (100, (GSourceFunc)release_lock, db);
    ck_assert_msg (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
    sqlite3_close (db);
}
END_TEST

static void
account_store_locked_cancel_cb (GObject *object, GAsyncResult *res,
                               gpointer user_data)
{
    gboolean *called = user_data;
    GError *error = NULL;

    g_debug ("%s called", G_STRFUNC);

    ag_account_store_finish (AG_ACCOUNT (object), res, &error);
    ck_assert_msg (error != NULL, "Account disposed but no error set!");
    ck_assert_msg (error->domain == G_IO_ERROR, "Wrong error domain");
    ck_assert_msg (error->code == G_IO_ERROR_CANCELLED,
                   "Got a different error code");
    g_error_free (error);
    *called = TRUE;
}

static gboolean
release_lock_cancel (sqlite3 *db)
{
    g_debug ("releasing lock");
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);

    end_test ();
    return FALSE;
}

static gboolean
cancel_store (gpointer user_data)
{
    GCancellable *cancellable = user_data;

    g_debug ("Cancelling %p", cancellable);
    g_cancellable_cancel (cancellable);
    return FALSE;
}

START_TEST(test_store_locked_cancel)
{
    sqlite3 *db;
    GCancellable *cancellable;
    gboolean cb_called = FALSE;

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    cancellable = g_cancellable_new ();
    ag_account_store_async (account, cancellable, account_store_locked_cancel_cb, &cb_called);
    g_timeout_add (100, (GSourceFunc)cancel_store, cancellable);
    g_timeout_add (200, (GSourceFunc)release_lock_cancel, db);
    ck_assert_msg (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
    ck_assert_msg (cb_called, "Callback not invoked");
    sqlite3_close (db);
    g_object_unref (cancellable);
}
END_TEST

static gboolean
test_store_read_only_handle_store_cb (TestManager *test_manager,
                                      GDBusMethodInvocation *invocation,
                                      guint account_id,
                                      gboolean created,
                                      gboolean deleted,
                                      const gchar *provider,
                                      GVariant *settings,
                                      StoreCbData *store_data)
{
    g_debug ("%s called", G_STRFUNC);
    guint result_id = store_data->account_id;

    store_data->called = TRUE;
    store_data->account_id = account_id;
    store_data->created = created;
    store_data->deleted = deleted;
    store_data->provider = g_strdup (provider);
    store_data->settings = g_variant_ref (settings);
    test_manager_complete_store (test_manager, invocation, result_id);
    return TRUE;
}

static void
test_store_read_only_store_cb (GObject *object, GAsyncResult *res,
                               gpointer user_data)
{
    GMainLoop *loop = user_data;
    GError *error = NULL;

    g_debug ("%s called", G_STRFUNC);

    ag_account_store_finish (AG_ACCOUNT (object), res, &error);
    ck_assert (error == NULL);
    g_main_loop_quit (loop);
}

START_TEST(test_store_read_only)
{
    TestManager *test_manager;
    TestObjectSkeleton *test_object;
    StoreCbData store_data = { 0 };
    GDBusObjectManagerServer *object_manager;
    GDBusConnection *conn;
    const gchar *display_name = "My readonly account";
    const AgAccountId expected_id = 4;
    GError *error = NULL;
    guint reg_id;

    set_read_only ();

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Register the D-Bus account manager service */
    conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    ck_assert (error == NULL);
    ck_assert (conn != NULL);

    reg_id = g_bus_own_name_on_connection (conn,
                                           AG_MANAGER_SERVICE_NAME,
                                           G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                           NULL, NULL, NULL, NULL);

    test_manager = test_manager_skeleton_new ();
    g_signal_connect (test_manager, "handle-store",
                      G_CALLBACK (test_store_read_only_handle_store_cb),
                      &store_data);

    test_object = test_object_skeleton_new (AG_MANAGER_OBJECT_PATH);
    test_object_skeleton_set_manager (test_object, test_manager);

    object_manager =
        g_dbus_object_manager_server_new ("/com/google/code/AccountsSSO");
    g_dbus_object_manager_server_export (object_manager,
                                         G_DBUS_OBJECT_SKELETON (test_object));
    g_dbus_object_manager_server_set_connection (object_manager, conn);

    manager = ag_manager_new ();
    ck_assert (manager != NULL);

    /* create an account, write its display name */
    account = ag_manager_create_account (manager, "fakebook");
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");
    ag_account_set_display_name (account, display_name);

    /* We want to verify that the local account will be updated with this
     * ID */
    store_data.account_id = expected_id;
    ag_account_store_async (account, NULL,
                            test_store_read_only_store_cb,
                            main_loop);
    g_main_loop_run (main_loop);

    ck_assert (store_data.called);
    ck_assert (store_data.created);
    ck_assert (!store_data.deleted);
    ck_assert_uint_eq (store_data.account_id, 0);
    ck_assert_str_eq (store_data.provider, "fakebook");
    store_cb_data_unset (&store_data);

    ck_assert_uint_eq (account->id, expected_id);
    const char *name = ag_account_get_display_name (account);
    ck_assert (name != NULL);
    ck_assert_str_eq (name, display_name);

    /* cleaning up */
    g_object_unref (object_manager);
    g_object_unref (test_object);
    g_object_unref (test_manager);
    g_bus_unown_name (reg_id);
    g_object_unref (conn);

    /* delete the DB */
    g_object_unref (account);
    account = NULL;
    g_object_unref (manager);
    manager = NULL;

    delete_db ();

    end_test ();
}
END_TEST

void account_store_now_cb (AgAccount *account, const GError *error,
                           gpointer user_data)
{
    const gchar *string = user_data;

    ck_assert_msg (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        ck_abort_msg("Got error: %s", error->message);
    ck_assert_msg (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    data_stored = TRUE;
}

START_TEST(test_account_service)
{
    GValue value = { 0 };
    const gchar *description = "This is really a beautiful account";
    const gchar *display_name = "My test account";
    AgSettingSource source;
    AgAccountService *account_service;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, description);
    ag_account_set_value (account, "description", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_set_enabled (account, FALSE);
    ag_account_set_display_name (account, display_name);

    account_service = ag_account_service_new (account, service);
    ck_assert_msg (AG_IS_ACCOUNT_SERVICE (account_service),
                   "Failed to create AccountService");

    /* test the readable properties */
    {
        AgAccount *account_prop = NULL;
        AgService *service_prop = NULL;

        g_object_get (account_service,
                      "account", &account_prop,
                      "service", &service_prop,
                      NULL);
        ck_assert (account_prop == account);
        ck_assert (service_prop == service);
        g_object_unref (account_prop);
        ag_service_unref (service_prop);
    }

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_service_get_value (account_service, "parameters/port", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_PROFILE,
                   "Cannot get port from profile");
    ck_assert_msg (g_value_get_int (&value) == 5223,
                   "Wrong port number: %d", g_value_get_int (&value));

    g_value_unset (&value);

    /* test getters for account and service */
    ck_assert (ag_account_service_get_service (account_service) == service);
    ck_assert (ag_account_service_get_account (account_service) == account);

    g_object_unref (account_service);

    /* Test account service for global settings */
    account_service = ag_account_service_new (account, NULL);
    ck_assert_msg (AG_IS_ACCOUNT_SERVICE (account_service),
                   "Failed to create AccountService for global settings");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_service_get_value (account_service, "description", &value);
    ck_assert (source == AG_SETTING_SOURCE_ACCOUNT);
    ck_assert (g_strcmp0 (g_value_get_string (&value), description) == 0);
    g_value_unset (&value);

    g_object_unref (account_service);

    end_test ();
}
END_TEST

static void
on_account_service_enabled (AgAccountService *account_service,
                            gboolean enabled,
                            AccountServiceEnabledCbData *cb_data)
{
    ck_assert (ag_account_service_get_enabled (account_service) == enabled);
    cb_data->called_count++;
    cb_data->enabled = enabled;
}

START_TEST(test_account_service_enabledness)
{
    AgAccountId account_id;
    AgAccountService *account_service;
    gboolean service_enabled = FALSE;
    AccountServiceEnabledCbData enabled_signal = { 0, FALSE };
    GError *error = NULL;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_set_enabled (account, FALSE);

    account_service = ag_account_service_new (account, service);
    ck_assert_msg (AG_IS_ACCOUNT_SERVICE (account_service),
                   "Failed to create AccountService");

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    account_id = account->id;

    g_signal_connect (account_service, "enabled",
                      G_CALLBACK (on_account_service_enabled),
                      &enabled_signal);

    /* enable the service */
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* Still disabled, because the account is disabled */
    ck_assert (enabled_signal.enabled == FALSE);
    ck_assert (enabled_signal.called_count == 0);
    service_enabled = TRUE;
    g_object_get (account_service, "enabled", &service_enabled, NULL);
    ck_assert (service_enabled == FALSE);

    /* enable the account but disable the service*/
    ag_account_select_service (account, NULL);
    ag_account_set_enabled (account, TRUE);
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* Still disabled, because the account is disabled */
    ck_assert (enabled_signal.enabled == FALSE);
    ck_assert (enabled_signal.called_count == 0);

    /* Now enable the service */
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert (enabled_signal.enabled == TRUE);
    ck_assert (enabled_signal.called_count == 1);
    service_enabled = FALSE;
    g_object_get (account_service, "enabled", &service_enabled, NULL);
    ck_assert (service_enabled == TRUE);

    g_object_unref (account_service);

    ag_service_unref (service);
    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();

    /* reload the account and see that it's enabled */
    account = ag_manager_load_account (manager, account_id, &error);
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Couldn't load account %u", account_id);
    ck_assert_msg (error == NULL, "Error is not NULL");

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    /* load the global account, and check that it's enabled */
    account_service = ag_account_service_new (account, NULL);
    ck_assert (AG_IS_ACCOUNT_SERVICE (account_service));

    ck_assert (ag_account_service_get_enabled (account_service) == TRUE);
    g_object_unref (account_service);

    /* load the service, and check that it's enabled */
    account_service = ag_account_service_new (account, service);
    ck_assert_msg (AG_IS_ACCOUNT_SERVICE (account_service),
                   "Failed to create AccountService");

    g_signal_connect (account_service, "enabled",
                      G_CALLBACK (on_account_service_enabled),
                      &enabled_signal);

    ck_assert (ag_account_service_get_enabled (account_service) == TRUE);

    /* disable the service */
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert (enabled_signal.enabled == FALSE);

    g_object_unref (account_service);
    end_test ();
}
END_TEST

static void
on_account_service_changed (AgAccountService *account_service,
                            gchar ***fields)
{
    *fields = ag_account_service_get_changed_fields (account_service);
}

static gboolean
string_in_array (gchar **array, const gchar *string)
{
    gint i;
    gboolean found = FALSE;

    for (i = 0; array[i] != NULL; i++)
        if (g_strcmp0 (string, array[i]) == 0)
        {
            found = TRUE;
            break;
        }

    return found;
}

START_TEST(test_account_service_settings)
{
    AgAccountSettingIter iter, *dyn_iter;
    GValue value = { 0 };
    GVariant *variant;
    const gchar *username = "me@myhome.com";
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    const gchar *key;
    AgAccountService *account_service;
    AgSettingSource source;
    gchar **changed_fields = NULL;
    gint known_keys_count, total_keys_count;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_set_enabled (account, FALSE);
    ag_account_set_display_name (account, display_name);

    account_service = ag_account_service_new (account, service);
    ck_assert_msg (AG_IS_ACCOUNT_SERVICE (account_service),
                   "Failed to create AccountService");

    g_signal_connect (account_service, "changed",
                      G_CALLBACK (on_account_service_changed),
                      &changed_fields);

    /* enable the service */
    ag_account_set_enabled (account, TRUE);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, username);
    ag_account_service_set_value (account_service, "username", &value);
    g_value_unset (&value);

    variant = g_variant_new_boolean (check_automatically);
    ag_account_service_set_variant (account_service, "check_automatically",
                                    variant);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* The callback for the "changed" signal should have been emitted.
     * Let's check what changed fields were reported, and what their value
     * is now.
     */
    ck_assert (changed_fields != NULL);
    ck_assert (string_in_array (changed_fields, "username"));
    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_service_get_value (account_service, "username", &value);
    ck_assert (source == AG_SETTING_SOURCE_ACCOUNT);
    ck_assert (strcmp (g_value_get_string (&value), username) == 0);
    g_value_unset (&value);

    ck_assert (string_in_array (changed_fields, "check_automatically"));
    g_strfreev (changed_fields);

    /* Let's repeat the test, now that the settings are stored in the DB */
    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, check_automatically);
    ag_account_service_set_value (account_service, "check_automatically", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "Wednesday");
    ag_account_service_set_value (account_service, "day", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_service_set_value (account_service, "ForReal", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* The callback for the "changed" signal should have been emitted.
     * Let's check what changed fields were reported, and what their value
     * is now.
     */
    ck_assert (string_in_array (changed_fields, "check_automatically"));
    variant = ag_account_service_get_variant (account_service,
                                              "check_automatically", &source);
    ck_assert (source == AG_SETTING_SOURCE_ACCOUNT);
    ck_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN));
    ck_assert_int_eq (g_variant_get_boolean (variant), check_automatically);

    ck_assert (string_in_array (changed_fields, "day"));
    ck_assert (string_in_array (changed_fields, "ForReal"));
    g_strfreev (changed_fields);

    /* Enumerate the account service settings */
    known_keys_count = 0;
    total_keys_count = 0;
    ag_account_service_settings_iter_init (account_service, &iter, NULL);
    while (ag_account_settings_iter_get_next (&iter, &key, &variant))
    {
        ck_assert (key != NULL);
        ck_assert (variant != NULL);

        total_keys_count++;

        if (g_strcmp0 (key, "check_automatically") == 0)
        {
            known_keys_count++;
            ck_assert (g_variant_is_of_type (variant,
                                             G_VARIANT_TYPE_BOOLEAN));
            ck_assert_int_eq (g_variant_get_boolean (variant),
                              check_automatically);
        }
        else if (g_strcmp0 (key, "username") == 0)
        {
            known_keys_count++;
            ck_assert (g_variant_is_of_type (variant,
                                             G_VARIANT_TYPE_STRING));
            ck_assert_str_eq (g_variant_get_string (variant, NULL),
                              username);
        }
        else if (g_strcmp0 (key, "day") == 0)
        {
            known_keys_count++;
            ck_assert (g_variant_is_of_type (variant,
                                             G_VARIANT_TYPE_STRING));
            ck_assert_str_eq (g_variant_get_string (variant, NULL),
                              "Wednesday");
        }
        else if (g_strcmp0 (key, "ForReal") == 0)
        {
            known_keys_count++;
            ck_assert (g_variant_is_of_type (variant,
                                             G_VARIANT_TYPE_BOOLEAN));
            ck_assert_int_eq (g_variant_get_boolean (variant), TRUE);
        }
    }

    ck_assert_int_eq (known_keys_count, 4);

    /* Now try the same with the dynamically allocated iterator; let's just
     * check that it returns the same number of keys. */
    dyn_iter = ag_account_service_get_settings_iter (account_service, NULL);
    ck_assert (dyn_iter != NULL);

    while (ag_account_settings_iter_get_next (dyn_iter, &key, &variant))
    {
        total_keys_count--;
    }
    ck_assert_int_eq (total_keys_count, 0);

    g_boxed_free (ag_account_settings_iter_get_type (), dyn_iter);

    g_object_unref (account_service);
    end_test ();
}
END_TEST

static gboolean
account_service_in_list(GList *list, AgAccountId id, const gchar *service_name)
{
    while (list != NULL) {
        AgAccountService *account_service = AG_ACCOUNT_SERVICE(list->data);
        AgAccount *account;
        AgService *service;

        account = ag_account_service_get_account (account_service);
        service = ag_account_service_get_service (account_service);
        if (account->id == id &&
            g_strcmp0(ag_service_get_name (service), service_name) == 0)
            return TRUE;
        list = list->next;
    }

    return FALSE;
}

START_TEST(test_account_service_list)
{
    const gchar *display_name = "My test account";
#define N_ACCOUNTS  3
    AgAccountId account_id[N_ACCOUNTS];
    AgService *my_service, *my_service2;
    GList *list;
    gint i;

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new ();

    /* create a few accounts */
    for (i = 0; i < N_ACCOUNTS; i++)
    {
        account = ag_manager_create_account (manager, "maemo");
        ag_account_set_enabled (account, TRUE);
        ag_account_set_display_name (account, display_name);
        ag_account_store (account, account_store_now_cb, TEST_STRING);
        run_main_loop_for_n_seconds(0);
        ck_assert_msg (data_stored, "Callback not invoked immediately");
        data_stored = FALSE;
        account_id[i] = account->id;
        g_object_unref (account);
        account = NULL;
    }

    list = ag_manager_get_enabled_account_services (manager);
    ck_assert (list == NULL);

    list = ag_manager_get_account_services (manager);
    for (i = 0; i < N_ACCOUNTS; i++) {
        ck_assert (account_service_in_list (list,
                                            account_id[i], "MyService"));
        ck_assert (account_service_in_list (list,
                                            account_id[i], "MyService2"));
    }
    ck_assert_msg (g_list_length (list) == N_ACCOUNTS * 2,
                   "Got list length %d, expecting %d",
                   g_list_length (list), N_ACCOUNTS * 2);
    g_list_free_full (list, (GDestroyNotify)g_object_unref);


    /* Now add a few services, and play with the enabled flags */
    my_service = ag_manager_get_service (manager, "MyService");
    ck_assert (my_service != NULL);
    my_service2 = ag_manager_get_service (manager, "MyService2");
    ck_assert (my_service2 != NULL);

    account = ag_manager_get_account (manager, account_id[0]);
    ck_assert (AG_IS_ACCOUNT(account));
    ag_account_select_service (account, my_service);
    ag_account_set_enabled (account, TRUE);
    ag_account_select_service (account, my_service2);
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    g_object_unref (account);

    account = ag_manager_get_account (manager, account_id[1]);
    ck_assert (AG_IS_ACCOUNT(account));
    ag_account_set_enabled (account, FALSE);
    ag_account_select_service (account, my_service);
    ag_account_set_enabled (account, TRUE);
    ag_account_select_service (account, my_service2);
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    g_object_unref (account);

    account = ag_manager_get_account (manager, account_id[2]);
    ck_assert (AG_IS_ACCOUNT(account));
    ag_account_select_service (account, my_service);
    ag_account_set_enabled (account, FALSE);
    ag_account_select_service (account, my_service2);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    g_object_unref (manager);

    /* Now check if the list functions return the expected results */
    manager = ag_manager_new ();

    list = ag_manager_get_account_services (manager);
    for (i = 0; i < N_ACCOUNTS; i++) {
        ck_assert (account_service_in_list (list,
                                            account_id[i], "MyService"));
        ck_assert (account_service_in_list (list,
                                            account_id[i], "MyService2"));
    }
    ck_assert_msg (g_list_length (list) == N_ACCOUNTS * 2,
                   "Got list length %d, expecting %d",
                   g_list_length (list), N_ACCOUNTS * 2);
    g_list_free_full (list, (GDestroyNotify)g_object_unref);

    list = ag_manager_get_enabled_account_services (manager);
    ck_assert (account_service_in_list (list, account_id[0], "MyService"));
    ck_assert (account_service_in_list (list, account_id[2], "MyService2"));
    ck_assert_msg (g_list_length (list) == 2,
                   "Got list length %d, expecting %d",
                   g_list_length (list), 2);
    g_list_free_full (list, (GDestroyNotify)g_object_unref);

    g_object_unref (manager);

    /* Now try with a manager created for a specific service type */
    manager = ag_manager_new_for_service_type ("e-mail");

    list = ag_manager_get_account_services (manager);
    for (i = 0; i < N_ACCOUNTS; i++) {
        ck_assert (account_service_in_list (list,
                                            account_id[i], "MyService"));
    }
    ck_assert_msg (g_list_length (list) == N_ACCOUNTS,
                   "Got list length %d, expecting %d",
                   g_list_length (list), N_ACCOUNTS);
    g_list_free_full (list, (GDestroyNotify)g_object_unref);

    list = ag_manager_get_enabled_account_services (manager);
    ck_assert (account_service_in_list (list, account_id[0], "MyService"));
    ck_assert_msg (g_list_length (list) == 1,
                   "Got list length %d, expecting %d",
                   g_list_length (list), 1);
    g_list_free_full (list, (GDestroyNotify)g_object_unref);

    ag_service_unref (my_service);
    ag_service_unref (my_service2);

    end_test ();
}
END_TEST

static void
write_strings_to_account (AgAccount *account, const gchar *key_prefix,
                          const gchar **strings)
{
    GValue value = { 0, };
    gint i;

    /* first string is the key, second the value */
    for (i = 0; strings[i] != NULL; i += 2)
    {
        gchar *key = g_strdup_printf ("%s/%s", key_prefix, strings[i]);
        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, strings[i + 1]);
        ag_account_set_value (account, key, &value);
        g_value_unset (&value);
        g_free (key);
    }
}

static void
check_string_in_params (GHashTable *params,
                        const gchar *key, const gchar *expected)
{
    GValue *value;
    const gchar *actual;
    gboolean equal;

    value = g_hash_table_lookup (params, key);
    if (value == NULL)
    {
        if (expected == NULL) return;
        ck_abort_msg ("Key %s is missing", key);
    }

    actual = g_value_get_string (value);
    equal = (g_strcmp0 (actual, expected) == 0);
    if (!equal)
    {
        g_warning ("Values differ! Expected %s, actual %s", expected, actual);
    }

    ck_assert (equal);
}

START_TEST(test_auth_data)
{
    AgAccountId account_id;
    AgAccountService *account_service;
    AgService *my_service;
    const guint credentials_id = 0xdeadbeef;
    const gchar *method = "dummy-method";
    const gchar *mechanism = "dummy-mechanism";
    const gchar *global_params[] = {
        "id", "123",
        "service", "contacts",
        NULL
    };
    const gchar *service_params[] = {
        "display", "mobile",
        "service", TEST_SERVICE_VALUE,
        NULL
    };
    gchar *key_prefix;
    AgAuthData *data;
    GHashTable *params;
    GValue value = { 0, };

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new ();

    key_prefix = g_strdup_printf ("auth/%s/%s", method, mechanism);

    /* create a new account */
    account = ag_manager_create_account (manager, "maemo");
    ag_account_set_enabled (account, TRUE);
    write_strings_to_account (account, key_prefix, global_params);

    my_service = ag_manager_get_service (manager, "MyService");
    ck_assert (my_service != NULL);
    ag_account_select_service (account, my_service);
    ag_account_set_enabled (account, TRUE);
    write_strings_to_account (account, key_prefix, service_params);
    g_free (key_prefix);

    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, credentials_id);
    ag_account_set_value (account, "CredentialsId", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, method);
    ag_account_set_value (account, "auth/method", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, mechanism);
    ag_account_set_value (account, "auth/mechanism", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    account_id = account->id;
    g_object_unref (account);
    account = NULL;

    /* reload the account and get the AccountService */
    account = ag_manager_get_account (manager, account_id);
    ck_assert (AG_IS_ACCOUNT (account));
    account_service = ag_account_service_new (account, my_service);
    ck_assert (AG_IS_ACCOUNT_SERVICE (account_service));

    /* get the auth data and check its contents */
    data = ag_account_service_get_auth_data (account_service);
    ck_assert (data != NULL);
    ck_assert (ag_auth_data_get_credentials_id (data) == credentials_id);
    ck_assert (strcmp (ag_auth_data_get_method (data), method) == 0);
    ck_assert (strcmp (ag_auth_data_get_mechanism (data), mechanism) == 0);
    params = ag_auth_data_get_parameters (data);
    ck_assert (params != NULL);

    check_string_in_params (params, "id", "123");
    check_string_in_params (params, "display", "mobile");
    check_string_in_params (params, "service", TEST_SERVICE_VALUE);
    check_string_in_params (params, "from-provider", "yes");

    ag_auth_data_unref (data);
    g_object_unref (account_service);
    ag_service_unref (my_service);

    end_test ();
}
END_TEST

static void
check_variant_in_dict (GVariant *dict, const gchar *key,
                       GVariant *expected)
{
    GVariant *actual;

    actual = g_variant_lookup_value (dict, key, NULL);
    if (actual == NULL)
    {
        if (expected == NULL) return;
        ck_abort_msg ("Key %s is missing", key);
    }

    if (!g_variant_equal(actual, expected))
    {
        ck_abort_msg ("Values differ for key %s! Expected %s, actual %s", key,
                      g_variant_print (expected, TRUE),
                      g_variant_print (actual, TRUE));
    }

    g_variant_ref_sink (expected);
    g_variant_unref (expected);
    g_variant_unref (actual);
}

START_TEST(test_auth_data_get_login_parameters)
{
    GList *account_services;
    AgAccountService *account_service;
    AgAuthData *data;
    GVariant *params, *variant;
    GVariantBuilder builder;
    const gchar *display = "desktop";
    const gchar *animal = "cat";

    manager = ag_manager_new_for_service_type ("e-mail");

    /* first, check the default parameters on a non-stored account */
    account = ag_manager_create_account (manager, "maemo");
    account_service = ag_account_service_new (account, NULL);
    data = ag_account_service_get_auth_data (account_service);
    ck_assert (data != NULL);

    params = ag_auth_data_get_login_parameters (data, NULL);
    ck_assert (params != NULL);

    check_variant_in_dict (params, "id", g_variant_new_string ("879"));
    check_variant_in_dict (params, "display",
                           g_variant_new_string ("desktop"));
    check_variant_in_dict (params, "from-provider",
                           g_variant_new_string ("yes"));
    g_variant_unref (params);
    ag_auth_data_unref (data);
    data = NULL;
    g_clear_object (&account);
    g_clear_object (&account_service);

    /* reload the account and get the AccountService */
    account_services = ag_manager_get_account_services (manager);
    ck_assert (g_list_length(account_services) == 1);
    account_service = AG_ACCOUNT_SERVICE (account_services->data);
    ck_assert (AG_IS_ACCOUNT_SERVICE (account_service));
    g_list_free (account_services);

    /* get the auth data */
    data = ag_account_service_get_auth_data (account_service);
    ck_assert (data != NULL);

    /* add an application setting */
    params = ag_auth_data_get_login_parameters (data, NULL);
    ck_assert (params != NULL);

    check_variant_in_dict (params, "id", g_variant_new_string ("123"));
    check_variant_in_dict (params, "display",
                           g_variant_new_string ("mobile"));
    check_variant_in_dict (params, "service",
                           g_variant_new_string (TEST_SERVICE_VALUE));
    g_variant_unref (params);

    /* Try adding some client parameters */
    g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add (&builder, "{sv}",
                           "display", g_variant_new_string (display));
    g_variant_builder_add (&builder, "{sv}",
                           "animal", g_variant_new_string (animal));

    variant = g_variant_builder_end (&builder);
    params = ag_auth_data_get_login_parameters (data, variant);
    check_variant_in_dict (params, "id", g_variant_new_string ("123"));
    check_variant_in_dict (params, "display",
                           g_variant_new_string (display));
    check_variant_in_dict (params, "service",
                           g_variant_new_string (TEST_SERVICE_VALUE));
    check_variant_in_dict (params, "animal",
                           g_variant_new_string (animal));
    g_variant_unref (params);

    ag_auth_data_unref (data);
    g_object_unref (account_service);

    end_test ();
}
END_TEST
START_TEST(test_auth_data_insert_parameters)
{
    GList *account_services;
    AgAccountService *account_service;
    AgAuthData *data;
    GHashTable *params;
    GValue v_display = { 0, };
    GValue v_animal = { 0, };
    const gchar *display = "desktop";
    const gchar *animal = "cat";

    manager = ag_manager_new_for_service_type ("e-mail");

    /* reload the account and get the AccountService */
    account_services = ag_manager_get_account_services (manager);
    ck_assert (g_list_length(account_services) == 1);
    account_service = AG_ACCOUNT_SERVICE (account_services->data);
    ck_assert (AG_IS_ACCOUNT_SERVICE (account_service));
    g_list_free (account_services);

    /* get the auth data */
    data = ag_account_service_get_auth_data (account_service);
    ck_assert (data != NULL);

    /* add an application setting */
    params = g_hash_table_new (g_str_hash, g_str_equal);

    g_value_init (&v_display, G_TYPE_STRING);
    g_value_set_static_string (&v_display, display);
    g_hash_table_insert (params, "display", &v_display);

    g_value_init (&v_animal, G_TYPE_STRING);
    g_value_set_static_string (&v_animal, animal);
    g_hash_table_insert (params, "animal", &v_animal);

    ag_auth_data_insert_parameters (data, params);
    g_hash_table_unref (params);

    /* now check that the values are what we expect them to be */
    params = ag_auth_data_get_parameters (data);
    ck_assert (params != NULL);

    check_string_in_params (params, "animal", animal);
    check_string_in_params (params, "display", display);
    /* check the the other values are retained */
    check_string_in_params (params, "service", TEST_SERVICE_VALUE);

    ag_auth_data_unref (data);
    g_object_unref (account_service);

    end_test ();
}
END_TEST

START_TEST(test_application)
{
    AgService *email_service, *sharing_service;
    AgApplication *application;
    GDesktopAppInfo *app_info;
    GList *list;

    manager = ag_manager_new ();

    application = ag_manager_get_application (manager, "Mailer");
    ck_assert (application != NULL);
    ag_application_unref (application);

    email_service = ag_manager_get_service (manager, "MyService");
    ck_assert (email_service != NULL);

    sharing_service = ag_manager_get_service (manager, "OtherService");
    ck_assert (email_service != NULL);

    list = ag_manager_list_applications_by_service (manager, email_service);
    ck_assert (list != NULL);
    ck_assert_msg (g_list_length(list) == 1,
                   "Got %d applications, expecting 1", g_list_length(list));

    application = list->data;
    ck_assert (g_strcmp0 (ag_application_get_name (application),
                          "Mailer") == 0);
    ck_assert (g_strcmp0 (ag_application_get_i18n_domain (application),
                          "mailer-catalog") == 0);
    ck_assert (g_strcmp0 (ag_application_get_description (application),
                          "Mailer application") == 0);
    ck_assert (ag_application_supports_service (application, email_service));
    ck_assert (!ag_application_supports_service (application, sharing_service));
    ck_assert (g_strcmp0 (ag_application_get_service_usage (application,
                                                            email_service),
                          "Mailer can retrieve your e-mails") == 0);
    app_info = ag_application_get_desktop_app_info (application);
    ck_assert (G_IS_DESKTOP_APP_INFO (app_info));
    ck_assert (g_strcmp0 (g_app_info_get_display_name (G_APP_INFO (app_info)),
                          "Easy Mailer") == 0);
    g_object_unref (app_info);

    ag_application_unref (application);
    g_list_free (list);

    list = ag_manager_list_applications_by_service (manager, sharing_service);
    ck_assert (list != NULL);
    ck_assert_msg (g_list_length(list) == 1,
                   "Got %d applications, expecting 1", g_list_length(list));

    application = list->data;
    ck_assert (g_strcmp0 (ag_application_get_name (application),
                          "Gallery") == 0);
    ck_assert (g_strcmp0 (ag_application_get_description (application),
                          "Image gallery") == 0);
    ck_assert (!ag_application_supports_service (application, email_service));
    ck_assert (ag_application_supports_service (application, sharing_service));
    ck_assert (g_strcmp0 (ag_application_get_service_usage (application,
                                                            sharing_service),
                          "Publish images on OtherService") == 0);
    ag_application_unref (application);
    g_list_free (list);

    ag_service_unref (email_service);
    ag_service_unref (sharing_service);

    end_test ();
}
END_TEST

START_TEST(test_application_supported_services)
{
    AgService *email_service;
    AgApplication *application;
    GList *list;

    manager = ag_manager_new ();

    application = ag_manager_get_application (manager, "Mailer");
    ck_assert (application != NULL);

    list = ag_manager_list_services_by_application (manager, application);
    ck_assert (list != NULL);
    ck_assert_int_eq (g_list_length (list), 1);

    email_service = list->data;
    ck_assert (email_service != NULL);
    ck_assert_str_eq (ag_service_get_name (email_service), "MyService");

    ag_application_unref (application);
    ag_service_list_free (list);

    application = ag_manager_get_application (manager, "Gallery");
    ck_assert (application != NULL);

    list = ag_manager_list_services_by_application (manager, application);
    ck_assert (list != NULL);
    ck_assert_int_eq (g_list_length (list), 1);

    email_service = list->data;
    ck_assert_str_eq (ag_service_get_name (email_service), "OtherService");

    ag_application_unref (application);
    ag_service_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_service)
{
    GValue value = { 0 };
    AgService *service2;
    GList *tag_list, *list;
    AgAccountId account_id;
    const gchar *provider_name, *service_type, *service_name,
                *service_description, *icon_name;
    const gchar *description = "This is really a beautiful account";
    const gchar *username = "me@myhome.com";
    const gint interval = 30;
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    const gchar **string_list;
    const gchar *capabilities[] = {
        "chat",
        "file",
        "smileys",
        NULL
    };
    const gchar *animals[] = {
        "cat",
        "dog",
        "monkey",
        "snake",
        NULL
    };

    AgSettingSource source;
    GError *error = NULL;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ck_assert (ag_account_get_selected_service (account) == NULL);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, description);
    ag_account_set_value (account, "description", &value);
    g_value_unset (&value);

    service = ag_manager_get_service (manager, "MyUnexistingService");
    ck_assert (service == NULL);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    service_type = ag_service_get_service_type (service);
    ck_assert_msg (g_strcmp0 (service_type, "e-mail") == 0,
                   "Wrong service type: %s", service_type);

    service_name = ag_service_get_name (service);
    ck_assert_msg (g_strcmp0 (service_name, "MyService") == 0,
                   "Wrong service name: %s", service_name);

    service_name = ag_service_get_display_name (service);
    ck_assert_msg (g_strcmp0 (service_name, "My Service") == 0,
                   "Wrong service display name: %s", service_name);

    service_description = ag_service_get_description (service);
    ck_assert_msg (g_strcmp0 (service_description,
                   "My Service Description") == 0,
                   "Wrong service description: %s", service_description);

    icon_name = ag_service_get_icon_name (service);
    ck_assert_msg (g_strcmp0 (icon_name, "general_myservice") == 0,
                   "Wrong service icon name: %s", icon_name);

    ck_assert_str_eq (ag_service_get_i18n_domain (service), "myservice_i18n");

    tag_list = ag_service_get_tags (service);
    ck_assert (tag_list != NULL);
    for (list = tag_list; list != NULL; list = list->next)
    {
        const gchar *tag = list->data;
        g_debug(" Service tag: %s", tag);
        ck_assert_msg (g_strcmp0 (tag, "e-mail") == 0 ||
                       g_strcmp0 (tag, "messaging") == 0,
                       "Wrong service tag: %s", tag);
    }
    g_list_free (tag_list);
    ck_assert_msg (ag_service_has_tag (service, "e-mail"),
                   "Missing service tag");

    ag_account_set_enabled (account, FALSE);
    ag_account_set_display_name (account, display_name);

    ag_account_select_service (account, service);
    ck_assert_ptr_eq (ag_account_get_selected_service (account), service);

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "parameters/port", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_PROFILE,
                   "Cannot get port from profile");
    ck_assert_msg (g_value_get_int (&value) == 5223,
                   "Wrong port number: %d", g_value_get_int (&value));
    g_value_unset (&value);

    /* test getting a string list */
    g_value_init (&value, G_TYPE_STRV);
    source = ag_account_get_value (account, "parameters/capabilities", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_PROFILE,
                   "Cannot get capabilities from profile");
    string_list = g_value_get_boxed (&value);
    ck_assert_msg (test_strv_equal (capabilities, string_list),
                   "Wrong capabilties");
    g_value_unset (&value);

    /* enable the service */
    ag_account_set_enabled (account, TRUE);

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

    g_value_init (&value, G_TYPE_STRV);
    g_value_set_boxed (&value, animals);
    ag_account_set_value (account, "pets", &value);
    g_value_unset (&value);

    service2 = ag_manager_get_service (manager, "OtherService");

    tag_list = ag_service_get_tags (service2);
    ck_assert (tag_list != NULL);
    for (list = tag_list; list != NULL; list = list->next)
    {
        const gchar *tag = list->data;
        g_debug(" Service tag: %s", tag);
        ck_assert_msg (g_strcmp0 (tag, "video") == 0 ||
                       g_strcmp0 (tag, "sharing") == 0,
                       "Wrong service tag: %s", tag);
    }
    g_list_free (tag_list);
    ck_assert_msg (ag_service_has_tag (service2, "sharing"),
                   "Missing service tag");
    
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
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    g_debug ("Account id: %d", account->id);
    account_id = account->id;

    ag_service_unref (service2);
    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();

    /* first, try to load an unexisting account */
    account = ag_manager_load_account (manager, account_id + 2, &error);
    ck_assert_msg (account == NULL, "Loading a non-existing account!");
    ck_assert_msg (error != NULL, "Error is NULL");
    g_clear_error (&error);

    account = ag_manager_load_account (manager, account_id, &error);
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Couldn't load account %u", account_id);
    ck_assert_msg (error == NULL, "Error is not NULL");

    provider_name = ag_account_get_provider_name (account);
    ck_assert_msg (g_strcmp0 (provider_name, PROVIDER) == 0,
                   "Got provider %s, expecting %s", provider_name, PROVIDER);

    /* check that the values are retained */
    ck_assert_msg (ag_account_get_enabled (account) == FALSE,
                   "Account enabled!");
    ck_assert_msg (g_strcmp0 (ag_account_get_display_name (account),
                            display_name) == 0,
                   "Display name not retained!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "description", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_strcmp0(g_value_get_string (&value), description) == 0,
                   "Wrong value");
    g_value_unset (&value);

    ag_account_select_service (account, service);

    /* we enabled the service before: check that it's still enabled */
    ck_assert_msg (ag_account_get_enabled (account) == TRUE,
                   "Account service not enabled!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "username", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_strcmp0(g_value_get_string (&value), username) == 0,
                   "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    source = ag_account_get_value (account, "check_automatically", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_value_get_boolean (&value) == check_automatically,
                   "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "interval", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_value_get_int (&value) == interval, "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRV);
    source = ag_account_get_value (account, "pets", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    string_list = g_value_get_boxed (&value);
    ck_assert_msg (test_strv_equal (string_list, animals),
                   "Wrong animals :-)");
    g_value_unset (&value);

    /* check also value conversion */
    g_value_init (&value, G_TYPE_CHAR);
    source = ag_account_get_value (account, "interval", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_value_get_schar (&value) == interval, "Wrong value");
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
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (ag_account_get_enabled (account) == TRUE,
                   "Account still disabled!");
    end_test ();
}
END_TEST

static gboolean
service_in_list(GList *list, const gchar *service_name)
{
    while (list != NULL) {
        AgService *service = list->data;

        if (g_strcmp0(ag_service_get_name (service), service_name) == 0)
            return TRUE;
        list = list->next;
    }

    return FALSE;
}

START_TEST(test_account_services)
{
    GList *services;

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, "maemo");
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Failed to create the AgAccount.");

    services = ag_account_list_services (account);
    ck_assert (g_list_length (services) == 2);

    /* These should be MyService and Myservice2; the order is random */
    ck_assert (service_in_list(services, "MyService"));
    ck_assert (service_in_list(services, "MyService2"));

    ag_service_list_free (services);

    /* check that MyService is returned as a service supporting e-mail for
     * this account */
    services = ag_account_list_services_by_type (account, "e-mail");
    ck_assert (g_list_length (services) == 1);

    ck_assert (service_in_list(services, "MyService"));

    ag_service_list_free (services);

    /* check that the account supports the "e-mail" type (it's the type of
     * MyService */
    ck_assert (ag_account_supports_service (account, "e-mail") == TRUE);
    /* and doesn't support "sharing" */
    ck_assert (ag_account_supports_service (account, "sharing") == FALSE);

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
    gboolean notify_enabled_called = FALSE;
    gboolean notify_display_name_called = FALSE;
    gboolean enabled = FALSE;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_signal_connect_swapped (account, "enabled",
                              G_CALLBACK (set_boolean_variable),
                              &enabled_called);
    g_signal_connect_swapped (account, "display-name-changed",
                              G_CALLBACK (set_boolean_variable),
                              &display_name_called);
    g_signal_connect_swapped (account, "notify::enabled",
                              G_CALLBACK (set_boolean_variable),
                              &notify_enabled_called);
    g_signal_connect_swapped (account, "notify::display-name",
                              G_CALLBACK (set_boolean_variable),
                              &notify_display_name_called);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);


    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");

    ck_assert_msg (enabled_called, "Enabled signal not emitted!");
    ck_assert_msg (display_name_called, "DisplayName signal not emitted!");
    ck_assert_msg (notify_enabled_called, "Enabled property not notified!");
    g_object_get (account, "enabled", &enabled, NULL);
    ck_assert_msg (enabled == TRUE, "Account not enabled!");
    ck_assert_msg (notify_display_name_called,
                   "DisplayName property not notified!");

    end_test ();
}
END_TEST

START_TEST(test_signals_other_manager)
{
    AgAccountId account_id;
    AgManager *manager2;
    AgAccount *account2;
    EnabledCbData ecd;
    GError *error = NULL;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    account_id = account->id;

    manager2 = ag_manager_new ();

    /* reload the account and see that it's enabled */
    account2 = ag_manager_load_account (manager2, account_id, &error);
    ck_assert_msg (AG_IS_ACCOUNT (account2),
                   "Couldn't load account %u", account_id);
    ck_assert_msg (error == NULL, "Error is not NULL");

    memset(&ecd, 0, sizeof(ecd));
    g_signal_connect (account2, "enabled",
                      G_CALLBACK (on_enabled),
                      &ecd);

    /* enable the service */
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    main_loop = g_main_loop_new (NULL, FALSE);
    g_timeout_add_seconds (2, quit_loop, main_loop);
    g_signal_connect_swapped (account2, "enabled",
                              G_CALLBACK (quit_loop), main_loop);
    g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);
    main_loop = NULL;

    ck_assert (ecd.called);
    ck_assert (g_strcmp0 (ecd.service, "MyService") == 0);
    g_free (ecd.service);

    ag_service_unref (service);
    service = NULL;
    g_object_unref (account2);
    g_object_unref (manager2);

    end_test ();
}
END_TEST

START_TEST(test_list)
{
    const gchar *display_name = "New account";
    const gchar *provider_name = "other_provider";
    const gchar *my_service_name = "MyService";
    const gchar *service_name = "OtherService";
    const gchar *service_type;
    GList *list;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, provider_name);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (account->id != 0, "Account ID is still 0!");

    /* Test the account readable properties */
    {
        AgAccountId id_prop = 0;
        AgManager *manager_prop = NULL;
        gchar *provider_prop = NULL;

        g_object_get (account,
                      "id", &id_prop,
                      "manager", &manager_prop,
                      "provider", &provider_prop,
                      NULL);
        ck_assert (id_prop == account->id);
        ck_assert (manager_prop == manager);
        ck_assert (g_strcmp0 (provider_prop, provider_name) == 0);
        g_object_unref (manager);
        g_free (provider_prop);
    }

    list = ag_manager_list (manager);
    ck_assert_msg (list != NULL, "Empty list");
    ck_assert_msg (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                   "Created account not found in list");
    g_list_free (list);

    /* check that it doesn't support the service type provided by MyService */
    service = ag_manager_get_service (manager, my_service_name);
    service_type = ag_service_get_service_type (service);
    ck_assert_msg (service_type != NULL,
                   "Service %s has no type", my_service_name);

    list = ag_manager_list_by_service_type (manager, service_type);
    ck_assert_msg (g_list_find (list, GUINT_TO_POINTER (account->id)) == NULL,
                   "New account supports %s service type, but shouldn't",
                   service_type);
    g_list_free (list);
    ag_service_unref(service);

    service = ag_manager_get_service (manager, service_name);
    service_type = ag_service_get_service_type (service);
    ck_assert_msg (service_type != NULL,
                   "Service %s has no type", service_name);

    list = ag_manager_list_by_service_type (manager, service_type);
    ck_assert_msg (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                   "New account doesn't supports %s service type, but should",
                   service_type);
    g_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_settings_iter_gvalue)
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
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (account->id != 0, "Account ID is still 0!");

    /* iterate the settings */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                ck_assert_msg (g_strcmp0 (values[i], text) == 0,
                               "Got value %s for key %s, expecting %s",
                               text, key, values[i]);
                break;
            }
        }

        ck_assert_msg (found, "Unknown setting %s", key);

        n_read++;
    }

    ck_assert_msg (n_read == n_values,
                   "Not all settings were retrieved (%d out of %d)",
                   n_read, n_values);

    /* iterate settings with prefix */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "param/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        gchar *full_key;
        ck_assert_msg (strncmp (key, "param/", 6) != 0,
                       "Got key with unstripped prefix (%s)", key);

        full_key = g_strconcat ("param/", key, NULL);
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (full_key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                ck_assert_msg (g_strcmp0 (values[i], text) == 0,
                               "Got value %s for key %s, expecting %s",
                               text, key, values[i]);
                break;
            }
        }
        g_free (full_key);

        ck_assert_msg (found, "Unknown setting %s", key);

        n_read++;
    }

    ck_assert_msg (n_read == 3, "Not all settings were retrieved");

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
    ck_assert_msg (n_read == 4, "Not all settings were retrieved");

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
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* enumerate the parameters */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "parameters/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        ck_assert_msg (strncmp (key, "parameters/", 6) != 0,
                       "Got key with unstripped prefix (%s)", key);

        g_debug ("Got key %s of type %s", key, G_VALUE_TYPE_NAME (val));
        if (g_strcmp0 (key, "port") == 0)
        {
            gint port;

            port = g_value_get_int (val);
            ck_assert_msg (port == new_port_value,
                           "Got value %d for key %s, expecting %d",
                           port, key, new_port_value);
        }

        n_read++;
    }

    ck_assert_msg (n_read == 5, "Not all settings were retrieved");


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
    AgAccountSettingIter iter;
    const gchar *key;
    GVariant *val;
    gint i, n_values, n_read;
    const gint new_port_value = 32412;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ag_account_set_enabled (account, TRUE);

    for (i = 0; keys[i] != NULL; i++)
    {
        ag_account_set_variant (account, keys[i],
                                g_variant_new_string (values[i]));
    }
    n_values = i;

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (account->id != 0, "Account ID is still 0!");

    /* iterate the settings */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_get_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_variant_get_string (val, NULL);
                ck_assert_msg (g_strcmp0 (values[i], text) == 0,
                               "Got value %s for key %s, expecting %s",
                               text, key, values[i]);
                break;
            }
        }

        ck_assert_msg (found, "Unknown setting %s", key);

        n_read++;
    }

    ck_assert_msg (n_read == n_values,
                   "Not all settings were retrieved (%d out of %d)",
                   n_read, n_values);

    /* iterate settings with prefix */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "param/");
    while (ag_account_settings_iter_get_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        gchar *full_key;
        ck_assert_msg (strncmp (key, "param/", 6) != 0,
                       "Got key with unstripped prefix (%s)", key);

        full_key = g_strconcat ("param/", key, NULL);
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (full_key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_variant_get_string (val, NULL);
                ck_assert_msg (g_strcmp0 (values[i], text) == 0,
                               "Got value %s for key %s, expecting %s",
                               text, key, values[i]);
                break;
            }
        }
        g_free (full_key);

        ck_assert_msg (found, "Unknown setting %s", key);

        n_read++;
    }

    ck_assert_msg (n_read == 3, "Not all settings were retrieved");

    /* iterate template default settings */
    service = ag_manager_get_service (manager, service_name);
    ag_account_select_service (account, service);
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_get_next (&iter, &key, &val))
    {
        g_debug ("Got key %s of type %s",
                 key, g_variant_get_type_string (val));

        n_read++;
    }
    ck_assert_msg (n_read == 4, "Not all settings were retrieved");

    /* Add a setting that is also on the template, to check if it will
     * override the one on the template */
    ag_account_set_variant (account, "parameters/port",
                            g_variant_new_int16 (new_port_value));

    /* Add a setting */
    ag_account_set_variant (account, "parameters/message",
                            g_variant_new_string ("How's life?"));

    /* save */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* enumerate the parameters */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "parameters/");
    while (ag_account_settings_iter_get_next (&iter, &key, &val))
    {
        ck_assert_msg (strncmp (key, "parameters/", 6) != 0,
                       "Got key with unstripped prefix (%s)", key);

        g_debug ("Got key %s of type %s",
                 key, g_variant_get_type_string (val));
        if (g_strcmp0 (key, "port") == 0)
        {
            gint port;

            port = g_variant_get_int16 (val);
            ck_assert_msg (port == new_port_value,
                           "Got value %d for key %s, expecting %d",
                           port, key, new_port_value);
        }

        n_read++;
    }

    ck_assert_msg (n_read == 5, "Not all settings were retrieved");


    end_test ();
}
END_TEST

START_TEST(test_list_services)
{
    GList *services, *list;
    gint n_services;
    AgService *service;
    const gchar *name;

    manager = ag_manager_new ();

    /* get all services */
    services = ag_manager_list_services (manager);

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 3, "Got %d services, expecting 3", n_services);

    for (list = services; list != NULL; list = list->next)
    {
        service = list->data;

        name = ag_service_get_name (service);
        g_debug ("Service name: %s", name);
        ck_assert_msg (g_strcmp0 (name, "MyService") == 0 ||
                       g_strcmp0 (name, "MyService2") == 0 ||
                       g_strcmp0 (name, "OtherService") == 0,
                       "Got unexpected service `%s'", name);
    }
    ag_service_list_free (services);

    /* get services by type */
    services = ag_manager_list_services_by_type (manager, "sharing");

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 1, "Got %d services, expecting 1", n_services);

    list = services;
    service = list->data;
    name = ag_service_get_name (service);
    ck_assert_msg (g_strcmp0 (name, "OtherService") == 0,
                   "Got unexpected service `%s'", name);
    ag_service_list_free (services);

    end_test ();
}
END_TEST

START_TEST(test_list_service_types)
{
    GList *service_types, *list, *tags, *tag_list;
    gint n_service_types;
    AgServiceType *service_type;
    const gchar *name, *tag;

    manager = ag_manager_new ();

    service_types = ag_manager_list_service_types (manager);

    n_service_types = g_list_length (service_types);
    ck_assert_msg (n_service_types == 1,
                   "Got %d service types, expecting 1",
                   n_service_types);

    for (list = service_types; list != NULL; list = list->next)
    {
        service_type = list->data;

        name = ag_service_type_get_name (service_type);
        g_debug ("Service type name: %s", name);
        ck_assert_msg (g_strcmp0 (name, "e-mail") == 0,
                       "Got unexpected service type `%s'", name);
        
        tags = ag_service_type_get_tags (service_type);
        for (tag_list = tags; tag_list != NULL; tag_list = tag_list->next)
        {
            tag = (gchar *) tag_list->data;
            g_debug (" Service type tag: %s", tag);
            ck_assert_msg ((g_strcmp0 (tag, "e-mail") == 0 ||
                            g_strcmp0 (tag, "messaging") == 0),
                           "Got unexpected service type tag `%s'", tag);
        }
        g_list_free (tags);
        ck_assert_msg (ag_service_type_has_tag (service_type, "messaging"),
                       "Missing service type tag");
    }
    ag_service_type_list_free (service_types);

    end_test ();
}
END_TEST


START_TEST(test_delete)
{
    AgAccountId id;
    gboolean enabled_called, deleted_called;

    manager = ag_manager_new ();

    /* create an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (account->id != 0, "Account ID is still 0!");
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
    ck_assert_msg (enabled_called == FALSE, "Accound disabled too early!");
    ck_assert_msg (deleted_called == FALSE, "Accound deleted too early!");

    /* really delete the account */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* check that the signals are emitted */
    ck_assert_msg (enabled_called, "Accound enabled signal not emitted");
    ck_assert_msg (deleted_called, "Accound deleted signal not emitted");

    g_object_unref (account);

    /* load the account again: this must fail */
    account = ag_manager_get_account (manager, id);
    ck_assert_msg (account == NULL, "The account still exists");

    end_test ();
}
END_TEST

static void
key_changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    ck_assert (invoked != NULL);
    ck_assert_msg (*invoked == FALSE, "Callback invoked twice!");

    ck_assert (key != NULL);
    ck_assert_msg (g_strcmp0 (key, "parameters/server") == 0 ||
                   g_strcmp0 (key, "parameters/port") == 0,
                   "Callback invoked for wrong key %s", key);
    *invoked = TRUE;
}

static void
dir_changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    ck_assert (invoked != NULL);
    ck_assert_msg (*invoked == FALSE, "Callback invoked twice!");

    ck_assert (key != NULL);
    ck_assert_msg (g_strcmp0 (key, "parameters/") == 0,
                   "Callback invoked for wrong dir %s", key);
    *invoked = TRUE;
}

START_TEST(test_watches)
{
    gboolean server_changed = FALSE;
    gboolean port_changed = FALSE;
    gboolean dir_changed = FALSE;
    AgAccountWatch w_server, w_port, w_dir;
    GValue value = { 0 };

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_select_service (account, service);

    /* install some watches */
    w_server = ag_account_watch_key (account, "parameters/server",
                                     (AgAccountNotifyCb)key_changed_cb,
                                     &server_changed);
    ck_assert (w_server != NULL);

    w_port = ag_account_watch_key (account, "parameters/port",
                                   (AgAccountNotifyCb)key_changed_cb,
                                   &port_changed);
    ck_assert (w_port != NULL);

    w_dir = ag_account_watch_dir (account, "parameters/",
                                  (AgAccountNotifyCb)dir_changed_cb,
                                  &dir_changed);
    ck_assert (w_dir != NULL);

    /* change the port */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 22);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* if we didn't change the server, make sure the callback is not
     * invoked */
    ck_assert_msg (server_changed == FALSE, "Callback for 'server' invoked");

    /* make sure the port callback was called */
    ck_assert_msg (port_changed == TRUE, "Callback for 'port' not invoked");

    /* make sure the dir callback was called */
    ck_assert_msg (dir_changed == TRUE, "Callback for 'parameters/' not invoked");


    /* remove the watch for the port */
    ag_account_remove_watch (account, w_port);

    /* change two settings */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 25);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "warez.maemo.org");
    ag_account_set_value (account, "parameters/server", &value);
    g_value_unset (&value);

    server_changed = FALSE;
    port_changed = FALSE;
    dir_changed = FALSE;
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* make sure the callback for the server is invoked */
    ck_assert_msg (server_changed == TRUE, "Callback for 'server' not invoked");

    /* make sure the port callback was not called (we removed the watch) */
    ck_assert_msg (port_changed == FALSE, "Callback for 'port' invoked");

    /* make sure the dir callback was called */
    ck_assert_msg (dir_changed == TRUE, "Callback for 'parameters/' not invoked");

    end_test ();
}
END_TEST

START_TEST(test_no_dbus)
{
    gchar *bus_address;
    gboolean use_dbus = TRUE;

    /* Unset the DBUS_SESSION_BUS_ADDRESS variable, so that the connection
     * to D-Bus will fail.
     */
    bus_address = g_strdup (g_getenv ("DBUS_SESSION_BUS_ADDRESS"));
    g_unsetenv("DBUS_SESSION_BUS_ADDRESS");

    manager = g_initable_new (AG_TYPE_MANAGER, NULL, NULL,
                              "use-dbus", FALSE,
                              NULL);
    ck_assert_msg (manager != NULL, "AgManager creation failed even "
                   "with use-dbus set to FALSE");

    g_object_get (manager, "use-dbus", &use_dbus, NULL);
    ck_assert (!use_dbus);

    /* Test creating an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    ck_assert_msg (account->id != 0, "Account ID is still 0!");

    /* Restore the initial value */
    g_setenv ("DBUS_SESSION_BUS_ADDRESS", bus_address, TRUE);

    g_free (bus_address);

    end_test ();
}
END_TEST

static void
on_account_created (AgManager *manager, AgAccountId account_id,
                    AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    *id = account_id;
    g_main_loop_quit (main_loop);
}

static void
on_account_deleted (AgManager *manager, AgAccountId account_id,
                    AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    ck_assert_msg (account_id == *id, "Deletion of unexpected account");
    *id = 0;
    g_main_loop_quit (main_loop);
}

static void
changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    ck_assert (invoked != NULL);
    ck_assert_msg (*invoked == FALSE, "Callback invoked twice!");

    ck_assert (key != NULL);
    *invoked = TRUE;
    if (idle_finish == 0)
        idle_finish = g_idle_add ((GSourceFunc)g_main_loop_quit, main_loop);
}

static gboolean
concurrency_test_failed (gpointer userdata)
{
    g_debug ("Timeout");
    source_id = 0;
    g_main_loop_quit (main_loop);
    return FALSE;
}

START_TEST(test_concurrency)
{
    AgAccountId account_id;
    const gchar *provider_name, *display_name;
    gchar command[512];
    GValue value = { 0 };
    gboolean character_changed, string_changed, boolean_changed;
    gboolean unsigned_changed;
    AgSettingSource source;
    EnabledCbData ecd;
    gint ret;
    const gchar **string_list;
    const gchar *numbers[] = {
        "one",
        "two",
        "three",
        NULL
    };

    manager = ag_manager_new ();

    g_signal_connect (manager, "account-created",
                      G_CALLBACK (on_account_created), &account_id);

    account_id = 0;
    ret = system ("test-process create myprovider MyAccountName");
    ck_assert (ret != -1);

    main_loop = g_main_loop_new (NULL, FALSE);
    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_debug ("Running loop");
    g_main_loop_run (main_loop);

    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    ck_assert_msg (account_id != 0, "Account ID still 0");

    account = ag_manager_get_account (manager, account_id);
    ck_assert_msg (AG_IS_ACCOUNT (account), "Got invalid account");

    provider_name = ag_account_get_provider_name (account);
    ck_assert_msg (g_strcmp0 (provider_name, "myprovider") == 0,
                   "Wrong provider name '%s'", provider_name);

    display_name = ag_account_get_display_name (account);
    ck_assert_msg (g_strcmp0 (display_name, "MyAccountName") == 0,
                   "Wrong display name '%s'", display_name);

    {
        gchar *allocated_display_name = NULL;
        g_object_get (account,
                      "display-name", &allocated_display_name,
                      NULL);
        ck_assert_msg (g_strcmp0 (allocated_display_name, "MyAccountName") == 0,
                       "Wrong display name '%s'", allocated_display_name);
        g_free (allocated_display_name);
    }

    /* check deletion */
    g_signal_connect (manager, "account-deleted",
                      G_CALLBACK (on_account_deleted), &account_id);
    sprintf (command, "test-process delete %d", account_id);
    ret = system (command);
    ck_assert (ret != -1);

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);
    g_object_unref (account);

    ck_assert_msg (account_id == 0, "Account still alive");

    /* check a more complex creation */
    ret = system ("test-process create2 myprovider MyAccountName");
    ck_assert (ret != -1);

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    ck_assert_msg (account_id != 0, "Account ID still 0");

    account = ag_manager_get_account (manager, account_id);
    ck_assert_msg (AG_IS_ACCOUNT (account), "Got invalid account");

    ck_assert (ag_account_get_enabled (account) == TRUE);

    g_value_init (&value, G_TYPE_INT);
    ag_account_get_value (account, "integer", &value);
    ck_assert (g_value_get_int (&value) == -12345);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    ag_account_get_value (account, "string", &value);
    ck_assert (g_strcmp0 (g_value_get_string (&value), "a string") == 0);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRV);
    source = ag_account_get_value (account, "numbers", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    string_list = g_value_get_boxed (&value);
    ck_assert_msg (test_strv_equal (string_list, numbers),
                   "Wrong numbers");
    g_value_unset (&value);

    /* we expect more keys in MyService */
    service = ag_manager_get_service (manager, "MyService");
    ck_assert_msg (service != NULL, "Cannot get service");

    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_UINT);
    ag_account_get_value (account, "unsigned", &value);
    ck_assert (g_value_get_uint (&value) == 54321);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_CHAR);
    ag_account_get_value (account, "character", &value);
    ck_assert (g_value_get_schar (&value) == 'z');
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    ag_account_get_value (account, "boolean", &value);
    ck_assert (g_value_get_boolean (&value) == TRUE);
    g_value_unset (&value);

    ck_assert (ag_account_get_enabled (account) == FALSE);

    /* watch some key changes/deletions */
    ag_account_watch_key (account, "character",
                          (AgAccountNotifyCb)changed_cb,
                          &character_changed);

    ag_account_watch_key (account, "boolean",
                          (AgAccountNotifyCb)changed_cb,
                          &boolean_changed);

    ag_account_watch_key (account, "unsigned",
                          (AgAccountNotifyCb)changed_cb,
                          &unsigned_changed);

    ag_account_select_service (account, NULL);
    ag_account_watch_key (account, "string",
                          (AgAccountNotifyCb)changed_cb,
                          &string_changed);
    /* watch account enabledness */
    g_signal_connect (account, "enabled",
                      G_CALLBACK (on_enabled), &ecd);

    character_changed = boolean_changed = string_changed =
        unsigned_changed = FALSE;
    memset (&ecd, 0, sizeof (ecd));

    /* make changes remotely */
    sprintf (command, "test-process change %d", account_id);
    ret = system (command);
    ck_assert (ret != -1);

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    ck_assert (character_changed == TRUE);
    ck_assert (boolean_changed == TRUE);
    ck_assert (string_changed == TRUE);
    ck_assert (unsigned_changed == FALSE);

    g_value_init (&value, G_TYPE_STRING);
    ag_account_get_value (account, "string", &value);
    ck_assert (g_strcmp0 (g_value_get_string (&value),
                          "another string") == 0);
    g_value_unset (&value);

    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_CHAR);
    source = ag_account_get_value (account, "character", &value);
    ck_assert (source == AG_SETTING_SOURCE_NONE);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    ag_account_get_value (account, "boolean", &value);
    ck_assert (g_value_get_boolean (&value) == FALSE);
    g_value_unset (&value);

    ck_assert (ag_account_get_enabled (account) == TRUE);

    /* verify that the signal has been emitted correctly */
    ck_assert (ecd.called == TRUE);
    ck_assert (ecd.enabled_check == TRUE);
    ck_assert (g_strcmp0 (ecd.service, "MyService") == 0);
    g_free (ecd.service);

    end_test ();
}
END_TEST

START_TEST(test_service_regression)
{
    GValue value = { 0 };
    AgAccountId account_id;
    const gchar *provider_name;
    const gchar *username = "me@myhome.com";
    const gint interval = 30;
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    AgSettingSource source;

    /* This test is to catch a bug that happened when creating a new
     * account and settings some service values before setting the display
     * name. The store operation would fail with error "column name is not
     * unique" because for some reason the same service ended up being
     * written twice into the DB.
     */

    /* delete the database: this is essential because the bug was
     * reproducible only when the service was not yet in DB */
    g_unlink (db_filename);

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);

    ag_account_select_service (account, service);

    /* enable the service */
    ag_account_set_enabled (account, TRUE);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, username);
    ag_account_set_value (account, "username", &value);
    g_value_unset (&value);

    /* Change the display name (this is on the base account) */
    ag_account_set_display_name (account, display_name);

    /* and some more service settings */
    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, check_automatically);
    ag_account_set_value (account, "check_automatically", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, interval);
    ag_account_set_value (account, "interval", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    g_debug ("Account id: %d", account->id);
    account_id = account->id;

    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();
    account = ag_manager_get_account (manager, account_id);
    ck_assert_msg (AG_IS_ACCOUNT (account),
                   "Couldn't load account %u", account_id);

    provider_name = ag_account_get_provider_name (account);
    ck_assert_msg (g_strcmp0 (provider_name, PROVIDER) == 0,
                   "Got provider %s, expecting %s", provider_name, PROVIDER);

    /* check that the values are retained */
    ck_assert_msg (g_strcmp0 (ag_account_get_display_name (account),
                              display_name) == 0,
                   "Display name not retained!");

    ag_account_select_service (account, service);

    /* we enabled the service before: check that it's still enabled */
    ck_assert_msg (ag_account_get_enabled (account) == TRUE,
                   "Account service not enabled!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "username", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_strcmp0(g_value_get_string (&value), username) == 0,
                   "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    source = ag_account_get_value (account, "check_automatically", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_value_get_boolean (&value) == check_automatically,
                   "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "interval", &value);
    ck_assert_msg (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    ck_assert_msg (g_value_get_int (&value) == interval, "Wrong value");
    g_value_unset (&value);

    end_test ();
}
END_TEST

START_TEST(test_blocking)
{
    const gchar *display_name, *lock_filename;
    gchar command[512];
    gint timeout_ms, block_ms;
    GError *error = NULL;
    gboolean ok;
    struct timespec start_time, end_time;
    gint fd;
    gint ret;

    /* create an account */
    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);
    ck_assert (account != NULL);
    ag_account_set_display_name (account, "Blocked account");
    ok = ag_account_store_blocking (account, &error);
    ck_assert_msg (ok, "Got error %s", error ? error->message : "No error set");
    ck_assert (account->id != 0);

    display_name = ag_account_get_display_name (account);
    ck_assert_msg (g_strcmp0 (display_name, "Blocked account") == 0,
                   "Wrong display name '%s'", display_name);

    /* Now change the display name and make sure it's not updated
     * without storing :-) */
    ag_account_set_display_name (account, "Want to change");
    display_name = ag_account_get_display_name (account);
    ck_assert (g_strcmp0 (display_name, "Blocked account") == 0);


    /* Now start a process in the background to lock the DB for some time */

    /* first, create a lock file to synchronize the test */
    lock_filename = "/tmp/check_ag.lock";
    fd = open (lock_filename, O_CREAT | O_RDWR, 0666);

    timeout_ms = MAX_SQLITE_BUSY_LOOP_TIME_MS;

    sprintf (command, "test-process lock_db %d %s &",
             timeout_ms, lock_filename);
    ret = system (command);
    ck_assert (ret != -1);

    /* wait till the file is locked */
    while (lockf(fd, F_TEST, 0) == 0)
        sched_yield();

    clock_gettime (CLOCK_MONOTONIC, &start_time);
    ok = ag_account_store_blocking (account, &error);
    clock_gettime (CLOCK_MONOTONIC, &end_time);

    /* the operation completed successfully */
    ck_assert_msg (ok, "Got error %s", error ? error->message : "No error set");

    /* make sure the display name changed */
    display_name = ag_account_get_display_name (account);
    ck_assert (g_strcmp0 (display_name, "Want to change") == 0);

    /* make sure that we have been waiting for a reasonable time */
    block_ms = time_diff(&start_time, &end_time);
    g_debug ("Been blocking for %u ms", block_ms);

    /* With WAL journaling, the DB might be locked for a much shorter time
     * than what we expect. The following line would fail in that case:
     *
     * ck_assert (block_ms > timeout_ms - 100);
     *
     * Instead, let's just check that we haven't been locking for too long.
     */
    ck_assert (block_ms < timeout_ms + 10000);

    end_test ();
}
END_TEST

START_TEST(test_cache_regression)
{
    AgAccountId account_id;
    const gchar *provider1 = "first_provider";
    const gchar *provider2 = "second_provider";
    const gchar *display_name1 = "first_displayname";
    const gchar *display_name2 = "second_displayname";
    AgAccount *deleted_account;

    /* This test is to catch a bug that happened when deleting the account
     * with the highest ID without letting the object die, and creating a
     * new account: the account manager would still return the old account!
     */

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, provider1);
    ck_assert (account != NULL);

    ag_account_set_display_name (account, display_name1);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    account_id = account->id;

    /* now remove the account, but don't destroy the object */
    ag_account_delete (account);
    deleted_account = account;

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* after deleting the account, we shouldn't get it anymore, even if we
     * didn't release our reference */
    account = ag_manager_get_account (manager, account_id);
    ck_assert (account == NULL);

    /* create another account */
    account = ag_manager_create_account (manager, provider2);
    ck_assert (account != NULL);

    ag_account_set_display_name (account, display_name2);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* check that the values are the correct ones */
    ck_assert (g_strcmp0 (ag_account_get_display_name (account),
                          display_name2) == 0);

    ck_assert (g_strcmp0 (ag_account_get_provider_name (account),
                          provider2) == 0);

    g_object_unref (deleted_account);

    end_test ();
}
END_TEST

START_TEST(test_serviceid_regression)
{
    AgAccount *account1, *account2;
    AgManager *manager1, *manager2;
    AgService *service1, *service2;
    const gchar *provider = "first_provider";

    /* This test is to catch a bug that happened when creating two accounts
     * having the same service, from two different instances of the
     * manager: the creation of the second account would fail.
     * Precondition: empty DB.
     */

    /* delete the database */
    g_unlink (db_filename);

    manager1 = ag_manager_new ();
    manager2 = ag_manager_new ();

    account1 = ag_manager_create_account (manager1, provider);
    ck_assert (account1 != NULL);
    account2 = ag_manager_create_account (manager2, provider);
    ck_assert (account2 != NULL);

    service1 = ag_manager_get_service (manager1, "MyService");
    ck_assert (service1 != NULL);
    service2 = ag_manager_get_service (manager2, "MyService");
    ck_assert (service2 != NULL);

    ag_account_select_service (account1, service1);
    ag_account_set_enabled (account1, TRUE);
    ag_account_select_service (account2, service2);
    ag_account_set_enabled (account2, FALSE);

    ag_account_store (account1, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ag_account_store (account2, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert (account1->id != 0);
    ck_assert (account2->id != 0);

    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    g_object_unref (account1);
    g_object_unref (account2);
    g_object_unref (manager1);
    g_object_unref (manager2);

    end_test ();
}
END_TEST

START_TEST(test_enabled_regression)
{
    EnabledCbData ecd;

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ck_assert (account != NULL);

    g_signal_connect (account, "enabled",
                      G_CALLBACK (on_enabled),
                      &ecd);

    memset (&ecd, 0, sizeof (ecd));
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, NULL, TEST_STRING);

    ck_assert (ecd.called == TRUE);
    ck_assert (ecd.service == NULL);
    ck_assert_msg (ecd.enabled_check == TRUE, "Settings are not updated!");

    memset (&ecd, 0, sizeof (ecd));
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, NULL, TEST_STRING);

    ck_assert (ecd.called == TRUE);
    ck_assert (ecd.service == NULL);
    ck_assert_msg (ecd.enabled_check == TRUE, "Settings are not updated!");

    end_test ();
}
END_TEST

START_TEST(test_delete_regression)
{
    AgAccountService *account_service;
    gboolean enabled_called, deleted_called;

    manager = ag_manager_new_for_service_type ("e-mail");

    /* create an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert_msg (account->id != 0, "Account ID is still 0!");

    account_service = ag_account_service_new (account, service);

    /* monitor the account status */
    g_signal_connect_swapped (account_service, "enabled",
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
    ck_assert_msg (enabled_called == FALSE, "Accound disabled too early!");
    ck_assert_msg (deleted_called == FALSE, "Accound deleted too early!");

    /* really delete the account */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    /* check that the signals are emitted */
    ck_assert_msg (enabled_called, "Accound enabled signal not emitted");
    ck_assert_msg (deleted_called, "Accound deleted signal not emitted");

    g_object_unref (account_service);

    end_test ();
}
END_TEST

static void
on_account_created_count (AgManager *manager, AgAccountId account_id,
                          gint *counter)
{
    g_debug ("%s called (%u), counter %d", G_STRFUNC, account_id, *counter);

    (*counter)++;
}

START_TEST(test_duplicate_create_regression)
{
    gint create_signal_counter;

    manager = ag_manager_new ();

    g_signal_connect (manager, "account-created",
                      G_CALLBACK (on_account_created_count),
                      &create_signal_counter);

    /* create an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);
    ag_service_unref(service);

    service = ag_manager_get_service (manager, "MyService2");
    ck_assert (service != NULL);
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    create_signal_counter = 0;
    ag_account_store_blocking (account, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    source_id = g_timeout_add_seconds (2, quit_loop, main_loop);
    g_debug ("Running loop");
    g_main_loop_run (main_loop);

    ck_assert_msg(create_signal_counter == 1,
                  "account-created emitted %d times!", create_signal_counter);

    end_test ();
}
END_TEST

START_TEST(test_manager_new_for_service_type)
{
    AgAccount *account1, *account2;
    AgService *service1, *service2;
    const gchar *provider = "first_provider";
    GList *list;

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new_for_service_type ("e-mail");
    ck_assert (g_strcmp0 (ag_manager_get_service_type (manager),
                          "e-mail") == 0);

    account1 = ag_manager_create_account (manager, provider);
    ck_assert (account1 != NULL);
    account2 = ag_manager_create_account (manager, provider);
    ck_assert (account2 != NULL);

    service1 = ag_manager_get_service (manager, "MyService");
    ck_assert (service1 != NULL);
    service2 = ag_manager_get_service (manager, "OtherService");
    ck_assert (service2 != NULL);

    ag_account_set_enabled (account1, TRUE);
    ag_account_select_service (account1, service1);
    ag_account_set_enabled (account1, TRUE);
    ag_account_set_enabled (account2, TRUE);
    ag_account_select_service (account2, service2);
    ag_account_set_enabled (account2, FALSE);

    ag_account_store (account1, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;
    ag_account_store (account2, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    ck_assert (account1->id != 0);
    ck_assert (account2->id != 0);

    list = ag_manager_list_enabled_by_service_type (manager, "e-mail");
    ck_assert (g_list_length (list) == 1);
    ck_assert (account1->id == GPOINTER_TO_UINT(list->data));

    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    g_object_unref (account1);
    g_object_unref (account2);
    ag_manager_list_free (list);

    end_test ();
}
END_TEST

static void
on_enabled_event (AgManager *manager, AgAccountId account_id,
                  AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);
    AgAccount *acc;
    AgService *service;

    acc = ag_manager_get_account (manager, account_id);
    ck_assert (acc != NULL);
    ck_assert (ag_account_get_enabled (acc));

    service = ag_manager_get_service (manager, "MyService");
    ck_assert (service != NULL);
    ag_account_select_service (acc, service);
    ck_assert (ag_account_get_enabled (acc));
    ag_service_unref (service);

    *id = account_id;

    g_object_unref (acc);
    g_main_loop_quit (main_loop);
}

static gboolean
enabled_event_test_failed (gpointer userdata)
{
    g_debug ("Timeout");
    source_id = 0;
    g_main_loop_quit (main_loop);
    return FALSE;
}

START_TEST(test_manager_enabled_event)
{
    gint ret;

    /* consume any still unprocessed D-Bus signals */
    run_main_loop_for_n_seconds (2);

    /* delete the database */
    g_unlink (db_filename);

    /* watch account enabledness */
    gchar command[512];
    AgAccountId account_id = 0;

    manager = ag_manager_new_for_service_type ("e-mail");
    ck_assert (manager != NULL);
    account = ag_manager_create_account (manager, "maemo");
    ck_assert (account != NULL);

    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    run_main_loop_for_n_seconds(0);
    ck_assert_msg (data_stored, "Callback not invoked immediately");
    data_stored = FALSE;

    main_loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (manager, "enabled-event",
                      G_CALLBACK (on_enabled_event), &account_id);

    /* this command will enable MyService (which is of type e-mail) */
    sprintf (command, "test-process enabled_event %d", account->id);
    ret = system (command);
    ck_assert (ret != -1);

    source_id = g_timeout_add_seconds (2, enabled_event_test_failed, NULL);
    g_main_loop_run (main_loop);
    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    ck_assert (account_id == account->id);

    account_id = 0;

    /* now disable the account. This also should trigger the enabled-event. */
    sprintf (command, "test-process enabled_event2 %d", account->id);
    ret = system (command);
    ck_assert (ret != -1);

    source_id = g_timeout_add_seconds (2, enabled_event_test_failed, NULL);
    g_main_loop_run (main_loop);
    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    ck_assert (account_id == account->id);

    end_test ();
}
END_TEST

START_TEST(test_list_enabled_account)
{
    GList *list = NULL;
    AgAccount *account1 = NULL;
    AgAccount *account2 = NULL;
    GList *iter = NULL;
    gboolean found = FALSE;
    AgAccount *account3 = NULL;
    const gchar *name = NULL;

    manager = ag_manager_new ();
    ck_assert_msg (manager != NULL, "Manager should not be NULL");

    account1 = ag_manager_create_account (manager, "MyProvider");
    ck_assert_msg (AG_IS_ACCOUNT (account1),
                   "Failed to create the AgAccount.");
    ag_account_set_display_name (account1, "EnabledAccount");
    ag_account_set_enabled (account1, TRUE);
    ag_account_store (account1, account_store_now_cb, TEST_STRING);

    account2 = ag_manager_create_account (manager, "MyProvider");
    ck_assert_msg (AG_IS_ACCOUNT (account2),
                   "Failed to create the AgAccount.");
    ag_account_set_display_name (account2, "DisabledAccount");
    ag_account_set_enabled (account2, FALSE);
    ag_account_store (account2, account_store_now_cb, TEST_STRING);


    list = ag_manager_list_enabled (manager);
    ck_assert_msg (g_list_length (list) > 0,
                   "No enabled accounts?");

    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        account3 = ag_manager_get_account (manager,
                                           GPOINTER_TO_UINT (iter->data));

        name = ag_account_get_display_name (account3);
        if (strcmp (name, "EnabledAccount") == 0)
        {
            found = TRUE;
            break;
        }
        g_object_unref (account3);
        account3 = NULL;
    }

    ck_assert_msg (found == TRUE, "Required account not enabled");

    if (account3)
        g_object_unref (account3);
    if (account2)
        g_object_unref (account2);
    if (account1)
        g_object_unref (account1);

    ag_manager_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_account_list_enabled_services)
{
    GList *services;
    gint n_services;
    AgService *service1, *service2;

    /*
     * Two additional managers:
     * manager2 : e-mail type
     * manager3 : sharing type
     * */
    AgManager *manager2, *manager3;

    /*
     * Same instances of account:
     * account2: from e-mail type manager
     * account3: from sharing manager
     * */
    AgAccount *account2, *account3;

    /*
     * new account for the same manager
     * */
    AgAccount *account4;

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new ();
    ck_assert (manager != NULL);

    manager2 = ag_manager_new_for_service_type ("e-mail");
    ck_assert (manager2 != NULL);

    manager3 = ag_manager_new_for_service_type ("sharing");
    ck_assert (manager3 != NULL);

    account = ag_manager_create_account (manager, "maemo");
    ck_assert (account != NULL);

    service1 = ag_manager_get_service (manager, "MyService");
    ck_assert (service1 != NULL);
    service2 = ag_manager_get_service (manager, "OtherService");
    ck_assert (service2 != NULL);

    /* 2 services, 1 enabled  */
    ag_account_select_service (account, service1);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);
    n_services = g_list_length (services);
    ck_assert_msg (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_service_list_free (services);

    /* 2 services, 2 enabled  */
    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 2, "Got %d services, expecting 2", n_services);
    ag_service_list_free (services);

    account2 = ag_manager_get_account (manager2, account->id);
    ck_assert (account2 != NULL);

    account3 = ag_manager_get_account (manager3, account->id);
    ck_assert (account3 != NULL);

    services = ag_account_list_enabled_services (account2);

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_service_list_free (services);

    services = ag_account_list_enabled_services (account3);

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_service_list_free (services);

    /* 2 services, 0 enabled  */
    account4 = ag_manager_create_account (manager, "maemo");
    ck_assert (account4 != NULL);

    ag_account_select_service (account, service1);
    ag_account_set_enabled (account, FALSE);

    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);

    ag_account_select_service (account4, service2);
    ag_account_set_enabled (account4, TRUE);
    ag_account_store (account4, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);

    n_services = g_list_length (services);
    ck_assert_msg (n_services == 0, "Got %d services, expecting 0", n_services);
    services = ag_account_list_enabled_services (account);
    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    ag_service_list_free (services);

    g_object_unref (account2);
    g_object_unref (account3);
    g_object_unref (account4);
    g_object_unref (manager2);
    g_object_unref (manager3);

    end_test ();
}
END_TEST

START_TEST(test_service_type)
{
    const gchar *string;
    AgServiceType *service_type;

    manager = ag_manager_new ();

    service_type = ag_manager_load_service_type (manager, "I don't exist");
    ck_assert (service_type == NULL);

    service_type = ag_manager_load_service_type (manager, "e-mail");
    ck_assert (service_type != NULL);

    string = ag_service_type_get_name (service_type);
    ck_assert_msg (g_strcmp0 (string, "e-mail") == 0,
                   "Wrong service type name: %s", string);

    string = ag_service_type_get_display_name (service_type);
    ck_assert_msg (g_strcmp0 (string, "Electronic mail") == 0,
                   "Wrong service type display name: %s", string);

    string = ag_service_type_get_description (service_type);
    ck_assert_msg (g_strcmp0 (string, "Electronic mail description") == 0,
                   "Wrong service type description: %s", string);

    string = ag_service_type_get_icon_name (service_type);
    ck_assert_msg (g_strcmp0 (string, "email_icon") == 0,
                   "Wrong service type icon name: %s", string);

    string = ag_service_type_get_i18n_domain (service_type);
    ck_assert_msg (g_strcmp0 (string, "translation_file") == 0,
                   "Wrong service type i18n name: %s", string);

    ag_service_type_unref (service_type);

    end_test ();
}
END_TEST

static void
on_account_created_with_db_locked (AgManager *manager, AgAccountId account_id)
{
    AgAccount *account;
    AgService *service;
    const gchar *name;
    GList *list;

    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    account = ag_manager_get_account (manager, account_id);
    ck_assert (account != NULL);

    g_debug ("account loaded");
    list = ag_account_list_enabled_services (account);
    ck_assert (list != NULL);
    ck_assert (g_list_length (list) == 1);

    service = list->data;
    ck_assert (service != NULL);

    name = ag_service_get_name (service);
    ck_assert (strcmp (name, "MyService") == 0);

    ag_service_list_free (list);
    g_main_loop_quit (main_loop);
}

START_TEST(test_db_access)
{
    const gchar *lock_filename;
    gchar command[512];
    gint timeout_ms;
    gint fd;
    gint ret;

    /* This test is for making sure that no DB accesses occur while certain
     * events occur.
     *
     * Checked scenarios:
     *
     * - when another process creates an account and we get the
     *   account-created signal and call
     *   ag_account_list_enabled_services(), we shouldn't be blocked.
     */

    /* first, create a lock file to synchronize the test */
    lock_filename = "/tmp/check_ag.lock";
    fd = open (lock_filename, O_CREAT | O_RDWR, 0666);

    timeout_ms = 2000; /* two seconds */

    manager = ag_manager_new ();
    ag_manager_set_db_timeout (manager, 0);
    ag_manager_set_abort_on_db_timeout (manager, TRUE);
    g_signal_connect (manager, "account-created",
                      G_CALLBACK (on_account_created_with_db_locked), NULL);

    /* create an account with the e-mail service type enabled */
    ret = system ("test-process create3 myprovider MyAccountName");
    ck_assert (ret != -1);

    /* lock the DB for the specified timeout */
    sprintf (command, "test-process lock_db %d %s &",
             timeout_ms, lock_filename);
    ret = system (command);
    ck_assert (ret != -1);

    /* wait till the file is locked */
    while (lockf (fd, F_TEST, 0) == 0)
        sched_yield ();

    /* now the DB is locked; we iterate the main loop to get the signals
     * about the account creation and do some operations with the account.
     * We expect to never get any error because of the locked DB, as the
     * methods we are calling should not access it */

    main_loop = g_main_loop_new (NULL, FALSE);
    source_id = g_timeout_add_seconds (timeout_ms / 1000,
                                       concurrency_test_failed, NULL);
    g_debug ("Running loop");
    g_main_loop_run (main_loop);

    ck_assert_msg (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    end_test ();
}
END_TEST

Suite *
ag_suite(const char *test_case)
{
    Suite *s = suite_create ("accounts-glib");

#define IF_TEST_CASE_ENABLED(test_name) \
    if (test_case == NULL || strcmp (test_name, test_case) == 0)

    TCase *tc;

    tc = tcase_create("Core");
    tcase_add_test (tc, test_init);
    tcase_add_test (tc, test_timeout_properties);
    IF_TEST_CASE_ENABLED("Core")
        suite_add_tcase (s, tc);

    tc = tcase_create("Create");
    tcase_add_test (tc, test_object);
    tcase_add_test (tc, test_read_only);
    IF_TEST_CASE_ENABLED("Create")
        suite_add_tcase (s, tc);

    tc = tcase_create("Provider");
    tcase_add_test (tc, test_provider);
    tcase_add_test (tc, test_provider_settings);
    tcase_add_test (tc, test_provider_directories);
    IF_TEST_CASE_ENABLED("Provider")
        suite_add_tcase (s, tc);

    tc = tcase_create("Store");
    tcase_add_test (tc, test_store);
    tcase_add_test (tc, test_store_locked);
    tcase_add_test (tc, test_store_locked_cancel);
    tcase_add_test (tc, test_store_read_only);
    IF_TEST_CASE_ENABLED("Store")
        suite_add_tcase (s, tc);

    tc = tcase_create("Service");
    tcase_add_test (tc, test_service);
    tcase_add_test (tc, test_account_services);
    tcase_add_test (tc, test_settings_iter_gvalue);
    tcase_add_test (tc, test_settings_iter);
    tcase_add_test (tc, test_service_type);
    IF_TEST_CASE_ENABLED("Service")
        suite_add_tcase (s, tc);

    tc = tcase_create("AccountService");
    tcase_add_test (tc, test_account_service);
    tcase_add_test (tc, test_account_service_enabledness);
    tcase_add_test (tc, test_account_service_settings);
    tcase_add_test (tc, test_account_service_list);
    IF_TEST_CASE_ENABLED("AccountService")
        suite_add_tcase (s, tc);

    tc = tcase_create("AuthData");
    tcase_add_test (tc, test_auth_data);
    tcase_add_test (tc, test_auth_data_get_login_parameters);
    tcase_add_test (tc, test_auth_data_insert_parameters);
    IF_TEST_CASE_ENABLED("AuthData")
        suite_add_tcase (s, tc);

    tc = tcase_create("Application");
    tcase_add_test (tc, test_application);
    tcase_add_test (tc, test_application_supported_services);
    IF_TEST_CASE_ENABLED("Application")
        suite_add_tcase (s, tc);

    tc = tcase_create("List");
    tcase_add_test (tc, test_list);
    tcase_add_test (tc, test_list_enabled_account);
    tcase_add_test (tc, test_list_services);
    tcase_add_test (tc, test_account_list_enabled_services);
    tcase_add_test (tc, test_list_service_types);
    IF_TEST_CASE_ENABLED("List")
        suite_add_tcase (s, tc);

    tc = tcase_create("Signalling");
    tcase_add_test (tc, test_signals);
    tcase_add_test (tc, test_signals_other_manager);
    tcase_add_test (tc, test_delete);
    tcase_add_test (tc, test_watches);
    IF_TEST_CASE_ENABLED("Signalling")
        suite_add_tcase (s, tc);

    tc = tcase_create("Concurrency");
    tcase_add_test (tc, test_no_dbus);
    tcase_add_test (tc, test_concurrency);
    tcase_add_test (tc, test_blocking);
    tcase_add_test (tc, test_manager_new_for_service_type);
    tcase_add_test (tc, test_manager_enabled_event);
    /* Tests for ensuring that opening and reading from a locked DB was
     * delayed have been removed since WAL journaling has been introduced:
     * they were failing, because with WAL journaling a writer does not
     * block readers.
     * Should we even need those tests back, they can be found in the git
     * history.
     */
    tcase_set_timeout (tc, 30);
    IF_TEST_CASE_ENABLED("Concurrency")
        suite_add_tcase (s, tc);

    tc = tcase_create("Regression");
    tcase_add_test (tc, test_service_regression);
    tcase_add_test (tc, test_cache_regression);
    tcase_add_test (tc, test_serviceid_regression);
    tcase_add_test (tc, test_enabled_regression);
    tcase_add_test (tc, test_delete_regression);
    tcase_add_test (tc, test_duplicate_create_regression);
    IF_TEST_CASE_ENABLED("Regression")
        suite_add_tcase (s, tc);

    tc = tcase_create("Caching");
    tcase_add_test (tc, test_db_access);
    tcase_set_timeout (tc, 10);
    IF_TEST_CASE_ENABLED("Caching")
        suite_add_tcase (s, tc);

    return s;
}

int main(int argc, char **argv)
{
    int number_failed;
    const char *test_case = NULL;

    if (argc > 1)
        test_case = argv[1];
    else
        test_case = g_getenv ("TEST_CASE");

    Suite * s = ag_suite(test_case);
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

