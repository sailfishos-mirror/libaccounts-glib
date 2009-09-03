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

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static GMainLoop *main_loop = NULL;
static AgAccount *account = NULL;
static AgManager *manager = NULL;
static AgService *service = NULL;

#define PROVIDER    "dummyprovider"

typedef struct {
    gint argc;
    gchar **argv;
} TestArgs;

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
}

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    if (error)
        g_warning ("Got error: %s", error->message);

    end_test ();
}

gboolean test_create (TestArgs *args)
{
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, args->argv[0]);

    if (args->argc > 1)
    {
        ag_account_set_display_name (account, args->argv[1]);
    }

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_delete (TestArgs *args)
{
    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi(args->argv[0]);
    account = ag_manager_get_account (manager, id);
    ag_account_delete (account);

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_create2 (TestArgs *args)
{
    GValue value = { 0 };

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, args->argv[0]);

    if (args->argc > 1)
    {
        ag_account_set_display_name (account, args->argv[1]);
    }

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, -12345);
    ag_account_set_value (account, "integer", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "a string");
    ag_account_set_value (account, "string", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);

    /* also set some keys in one service */
    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, 54321);
    ag_account_set_value (account, "unsigned", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_CHAR);
    g_value_set_char (&value, 'z');
    ag_account_set_value (account, "character", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_set_value (account, "boolean", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_change (TestArgs *args)
{
    GValue value = { 0 };

    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi(args->argv[0]);
    account = ag_manager_get_account (manager, id);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "another string");
    ag_account_set_value (account, "string", &value);
    g_value_unset (&value);

    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);

    ag_account_set_value (account, "character", NULL);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, FALSE);
    ag_account_set_value (account, "boolean", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

int main(int argc, char **argv)
{
    TestArgs args;

    g_type_init ();

    if (argc >= 2)
    {
        const gchar *test_name = argv[1];

        argc -= 2;
        argv += 2;
        args.argc = argc;
        args.argv = argv;

        if (strcmp (test_name, "create") == 0)
        {
            g_idle_add ((GSourceFunc)test_create, &args);
        }
        else if (strcmp (test_name, "delete") == 0)
        {
            g_idle_add ((GSourceFunc)test_delete, &args);
        }
        else if (strcmp (test_name, "create2") == 0)
        {
            g_idle_add ((GSourceFunc)test_create2, &args);
        }
        else if (strcmp (test_name, "change") == 0)
        {
            g_idle_add ((GSourceFunc)test_change, &args);
        }

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

