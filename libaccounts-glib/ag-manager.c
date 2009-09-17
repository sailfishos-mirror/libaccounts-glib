/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
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
 * SECTION:ag-manager
 * @title: AgManager
 * @short_description: The account manager object
 *
 * The #AgManager is the main object in this library.
 */

#include "ag-manager.h"

#include "ag-errors.h"
#include "ag-internals.h"
#include <dbus/dbus-glib-lowlevel.h>
#include <sched.h>
#include <sqlite3.h>
#include <string.h>

#define MAX_SQLITE_BUSY_LOOP_TIME 2

enum
{
    ACCOUNT_CREATED,
    ACCOUNT_DELETED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _AgManagerPrivate {
    sqlite3 *db;

    sqlite3_stmt *begin_stmt;
    sqlite3_stmt *commit_stmt;
    sqlite3_stmt *rollback_stmt;

    sqlite3_int64 last_service_id;
    sqlite3_int64 last_account_id;

    DBusConnection *dbus_conn;

    /* Cache for AgService */
    GHashTable *services;

    /* Weak references to loaded accounts */
    GHashTable *accounts;

    /* list of StoreCbData awaiting for exclusive locks */
    GList *locks;

    /* list of EmittedSignalData for the signals emitted by this instance */
    GList *emitted_signals;

    guint is_disposed : 1;
};

typedef struct {
    AgManager *manager;
    AgAccount *account;
    gchar *sql;
    AgAccountChanges *changes;
    guint id;
    AgAccountStoreCb callback;
    gpointer user_data;
} StoreCbData;

typedef struct {
    struct timespec ts;
    gboolean must_process;
} EmittedSignalData;

G_DEFINE_TYPE (AgManager, ag_manager, G_TYPE_OBJECT);

#define AG_MANAGER_PRIV(obj) (AG_MANAGER(obj)->priv)

static void store_cb_data_free (StoreCbData *sd);
static void account_weak_notify (gpointer userdata, GObject *dead_account);

static gboolean
timed_unref_account (gpointer account)
{
    g_debug ("Releasing temporary reference on account %u",
             AG_ACCOUNT (account)->id);
    g_object_unref (account);
    return FALSE;
}

static gboolean
parse_message_header (DBusMessageIter *iter,
                      struct timespec *ts, AgAccountId *id,
                      gboolean *created, gboolean *deleted,
                      const gchar **provider_name)
{
#define EXPECT_TYPE(t) \
    if (G_UNLIKELY (dbus_message_iter_get_arg_type (iter) != t)) return FALSE

    EXPECT_TYPE (DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic (iter, &ts->tv_sec);
    dbus_message_iter_next (iter);

    EXPECT_TYPE (DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic (iter, &ts->tv_nsec);
    dbus_message_iter_next (iter);

    EXPECT_TYPE (DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic (iter, id);
    dbus_message_iter_next (iter);

    EXPECT_TYPE (DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic (iter, created);
    dbus_message_iter_next (iter);

    EXPECT_TYPE (DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic (iter, deleted);
    dbus_message_iter_next (iter);

    EXPECT_TYPE (DBUS_TYPE_STRING);
    dbus_message_iter_get_basic (iter, provider_name);
    dbus_message_iter_next (iter);

#undef EXPECT_TYPE
    return TRUE;
}

static DBusHandlerResult
dbus_filter_callback (DBusConnection *dbus_conn, DBusMessage *msg,
                      void *user_data)
{
    AgManager *manager = AG_MANAGER (user_data);
    AgManagerPrivate *priv = manager->priv;
    const gchar *provider_name = NULL;
    AgAccountId account_id = 0;
    AgAccount *account;
    AgAccountChanges *changes;
    struct timespec ts;
    gboolean deleted, created;
    gboolean ret;
    gboolean ours = FALSE;
    DBusMessageIter iter;
    GList *list;

    if (!dbus_message_is_signal (msg, AG_DBUS_IFACE, AG_DBUS_SIG_CHANGED))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_init (msg, &iter);
    ret = parse_message_header (&iter, &ts, &account_id,
                                &created, &deleted, &provider_name);
    if (G_UNLIKELY (!ret))
    {
        g_warning ("%s: error in parsing signal arguments", G_STRFUNC);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    for (list = priv->emitted_signals; list != NULL; list = list->next)
    {
        EmittedSignalData *esd = list->data;

        if (esd->ts.tv_sec == ts.tv_sec &&
            esd->ts.tv_nsec == ts.tv_nsec)
        {
            /* message is ours: we can ignore it, as the changes
             * were already processed when the DB transaction succeeded. */
            ours = TRUE;

            g_debug ("Signal is ours, must_process = %d", esd->must_process);
            g_slice_free (EmittedSignalData, esd);
            priv->emitted_signals = g_list_delete_link (priv->emitted_signals,
                                                        list);
            if (!esd->must_process)
                goto nothing_to_do;
        }
    }

    /* we must mark our emitted signals for reprocessing, because the current
     * signal might modify some of the fields that were previously modified by
     * us.
     * This ensures that changes coming from different account manager
     * instances are processed in the right order. */
    for (list = priv->emitted_signals; list != NULL; list = list->next)
    {
        EmittedSignalData *esd = list->data;
        g_debug ("Marking pending signal for processing");
        esd->must_process = TRUE;
    }

    /* check if the account is loaded */
    account = g_hash_table_lookup (priv->accounts,
                                   GUINT_TO_POINTER (account_id));
    if (!account && !created && !deleted)
        goto nothing_to_do;

    if (ours && (deleted || created))
        goto nothing_to_do;

    if (!account)
    {
        /* because of the checks above, this can happen if this is an account
         * created or deleted from another instance.
         * We must emit the signals, and cache the newly created account for a
         * while, because the application is likely to inspect it */
        account = g_object_new (AG_TYPE_ACCOUNT,
                                "manager", manager,
                                "provider", provider_name,
                                "id", account_id,
                                NULL);
        g_return_val_if_fail (AG_IS_ACCOUNT (account),
                              DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

        g_object_weak_ref (G_OBJECT (account), account_weak_notify, manager);
        g_hash_table_insert (priv->accounts, GUINT_TO_POINTER (account_id),
                             account);
        g_timeout_add_seconds (2, timed_unref_account, account);
    }

    changes = _ag_account_changes_from_dbus (&iter, created, deleted);
    _ag_account_done_changes (account, changes);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

nothing_to_do:
    g_debug ("Nothing to do");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
signal_account_changes (AgManager *manager, AgAccount *account,
                        AgAccountChanges *changes)
{
    AgManagerPrivate *priv = manager->priv;
    DBusMessage *msg;
    gboolean ret;
    EmittedSignalData eds;

    clock_gettime(CLOCK_MONOTONIC, &eds.ts);

    msg = _ag_account_build_signal (account, changes, &eds.ts);
    if (G_UNLIKELY (!msg))
    {
        g_warning ("Creation of D-Bus signal failed");
        return;
    }

    ret = dbus_connection_send (priv->dbus_conn, msg, NULL);
    if (G_UNLIKELY (!ret))
    {
        g_warning ("Emission of DBus signal failed");
        goto finish;
    }

    dbus_connection_flush (priv->dbus_conn);
    g_debug ("Emitted signal, time: %lu-%lu", eds.ts.tv_sec, eds.ts.tv_nsec);

    eds.must_process = FALSE;
    priv->emitted_signals =
        g_list_prepend (priv->emitted_signals,
                        g_slice_dup (EmittedSignalData, &eds));

finish:
    dbus_message_unref (msg);
}

static gboolean
got_service (sqlite3_stmt *stmt, AgService **p_service)
{
    AgService *service;

    g_assert (p_service != NULL);

    service = _ag_service_new ();
    service->id = sqlite3_column_int (stmt, 0);
    service->display_name = g_strdup ((gchar *)sqlite3_column_text (stmt, 1));
    service->provider = g_strdup ((gchar *)sqlite3_column_text (stmt, 2));
    service->type = g_strdup ((gchar *)sqlite3_column_text (stmt, 3));

    *p_service = service;
    return TRUE;
}

static gboolean
add_id_to_list (sqlite3_stmt *stmt, GList **plist)
{
    gint id;

    id = sqlite3_column_int (stmt, 0);
    *plist = g_list_prepend (*plist, GINT_TO_POINTER (id));
    return TRUE;
}

static void
account_weak_notify (gpointer userdata, GObject *dead_account)
{
    AgManagerPrivate *priv = AG_MANAGER_PRIV (userdata);
    GHashTableIter iter;
    GObject *account;

    g_debug ("%s called for %p", G_STRFUNC, dead_account);
    g_hash_table_iter_init (&iter, priv->accounts);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer)&account))
    {
        if (account == dead_account)
        {
            g_hash_table_iter_steal (&iter);
            break;
        }
    }
}

static void
account_weak_unref (GObject *account)
{
    g_object_weak_unref (account, account_weak_notify,
                         ag_account_get_manager (AG_ACCOUNT (account)));
}

static void
transaction_completed (AgManager *manager,
                       AgAccount *account,
                       AgAccountChanges *changes,
                       AgAccountStoreCb callback,
                       const GError *error,
                       gpointer user_data)
{
    AgManagerPrivate *priv;

    g_debug ("%s called", G_STRFUNC);
    g_return_if_fail (AG_IS_MANAGER (manager));
    priv = manager->priv;

    if (callback)
        callback (account, error, user_data);

    if (!error)
    {
        /* TODO: emit D-Bus signal with the changes */
    }
    _ag_account_changes_free (changes);
}

/*
 * exec_transaction:
 *
 * Executes a transaction, assuming that the exclusive lock has been obtained.
 */
static void
exec_transaction (AgManager *manager, AgAccount *account,
                  const gchar *sql, AgAccountChanges *changes,
                  AgAccountStoreCb callback, gpointer user_data)
{
    AgManagerPrivate *priv;
    gchar *err_msg = NULL;
    GError *error = NULL;
    int ret;

    g_debug ("%s called: %s", G_STRFUNC, sql);
    g_return_if_fail (AG_IS_MANAGER (manager));
    priv = manager->priv;
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_return_if_fail (sql != NULL);
    g_return_if_fail (priv->db != NULL);

    ret = sqlite3_exec (priv->db, sql, NULL, NULL, &err_msg);
    if (G_UNLIKELY (ret != SQLITE_OK))
    {
        error = g_error_new_literal (AG_ERRORS, AG_ERROR_DB, err_msg);

        sqlite3_step (priv->rollback_stmt);
        sqlite3_reset (priv->rollback_stmt);
        goto finish;
    }

    ret = sqlite3_step (priv->commit_stmt);
    if (G_UNLIKELY (ret != SQLITE_DONE))
    {
        error = g_error_new_literal (AG_ERRORS, AG_ERROR_DB,
                                     sqlite3_errmsg (priv->db));
        goto finish;
    }

    /* everything went well; if this was a new account, we must update the
     * local data structure */
    if (account->id == 0)
    {
        account->id = priv->last_account_id;

        /* insert the account into our cache */
        g_object_weak_ref (G_OBJECT (account), account_weak_notify, manager);
        g_hash_table_insert (priv->accounts, GUINT_TO_POINTER (account->id),
                             account);
    }

    /* emit DBus signals to notify other processes */
    signal_account_changes (manager, account, changes);

    _ag_account_done_changes (account, changes);

finish:
    transaction_completed (manager, account, changes,
                           callback, error, user_data);
    if (G_UNLIKELY (error))
    {
        g_error_free (error);
        if (err_msg)
            sqlite3_free (err_msg);
    }
}

static void
lost_weak_ref (gpointer data, GObject *dead)
{
    StoreCbData *sd = data;
    AgManagerPrivate *priv;

    GError error = { AG_ERRORS, AG_ERROR_DISPOSED, "Account disposed" };

    g_assert ((GObject *)sd->account == dead);
    transaction_completed (sd->manager, sd->account, sd->changes,
                           sd->callback, &error, sd->user_data);

    priv = AG_MANAGER_PRIV (sd->manager);
    priv->locks = g_list_remove (priv->locks, sd);
    sd->account = NULL; /* so that the weak reference is not removed */
    store_cb_data_free (sd);
}

static void
store_cb_data_free (StoreCbData *sd)
{
    if (sd->account)
        g_object_weak_unref (G_OBJECT (sd->account), lost_weak_ref, sd);
    if (sd->id)
        g_source_remove (sd->id);
    g_free (sd->sql);
    g_slice_free (StoreCbData, sd);
}

static gboolean
exec_transaction_idle (StoreCbData *sd)
{
    AgManager *manager = sd->manager;
    AgAccount *account = sd->account;
    AgManagerPrivate *priv;
    int ret;

    g_return_val_if_fail (AG_IS_MANAGER (manager), FALSE);
    priv = manager->priv;

    g_return_val_if_fail (priv->begin_stmt != NULL, FALSE);
    ret = sqlite3_step (priv->begin_stmt);
    if (ret == SQLITE_BUSY)
    {
        sched_yield ();
        return TRUE; /* call this callback again */
    }

    g_object_ref (manager);
    g_object_ref (account);
    if (ret == SQLITE_DONE)
    {
        exec_transaction (manager, account, sd->sql, sd->changes,
                          sd->callback, sd->user_data);
    }
    else
    {
        GError error = { AG_ERRORS, AG_ERROR_DB, "Generic error" };
        transaction_completed (manager, account, sd->changes,
                               sd->callback, &error, sd->user_data);
    }

    priv->locks = g_list_remove (priv->locks, sd);
    sd->id = 0;
    store_cb_data_free (sd);
    g_object_unref (account);
    g_object_unref (manager);
    return FALSE;
}

static int
prepare_transaction_statements (AgManagerPrivate *priv)
{
    int ret;

    if (G_UNLIKELY (!priv->begin_stmt))
    {
        ret = sqlite3_prepare_v2 (priv->db, "BEGIN EXCLUSIVE;", -1,
                                  &priv->begin_stmt, NULL);
        if (ret != SQLITE_OK) return ret;
    }
    else
        sqlite3_reset (priv->begin_stmt);

    if (G_UNLIKELY (!priv->commit_stmt))
    {
        ret = sqlite3_prepare_v2 (priv->db, "COMMIT;", -1,
                                  &priv->commit_stmt, NULL);
        if (ret != SQLITE_OK) return ret;
    }
    else
        sqlite3_reset (priv->commit_stmt);

    if (G_UNLIKELY (!priv->rollback_stmt))
    {
        ret = sqlite3_prepare_v2 (priv->db, "ROLLBACK;", -1,
                                  &priv->rollback_stmt, NULL);
        if (ret != SQLITE_OK) return ret;
    }
    else
        sqlite3_reset (priv->rollback_stmt);

    return SQLITE_OK;
}

static void
set_last_rowid_as_service_id (sqlite3_context *ctx,
                              int argc, sqlite3_value **argv)
{
    AgManagerPrivate *priv;

    g_debug ("%s called", G_STRFUNC);
    priv = sqlite3_user_data (ctx);
    priv->last_service_id = sqlite3_last_insert_rowid (priv->db);
    if (argc == 1)
    {
        const guchar *service_name;
        AgService *service;

        service_name = sqlite3_value_text (argv[0]);

        service = g_hash_table_lookup (priv->services, service_name);
        if (G_LIKELY (service))
        {
            service->id = (gint)priv->last_service_id;
        }
        else
            g_warning ("Service %s not loaded", service_name);
    }

    sqlite3_result_null (ctx);
}

static void
set_last_rowid_as_account_id (sqlite3_context *ctx,
                              int argc, sqlite3_value **argv)
{
    AgManagerPrivate *priv;

    g_debug ("%s called", G_STRFUNC);
    priv = sqlite3_user_data (ctx);
    priv->last_account_id = sqlite3_last_insert_rowid (priv->db);
    sqlite3_result_null (ctx);
}

static void
get_service_id (sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
    AgManagerPrivate *priv;

    priv = sqlite3_user_data (ctx);
    sqlite3_result_int64 (ctx, priv->last_service_id);
}

static void
get_account_id (sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
    AgManagerPrivate *priv;

    priv = sqlite3_user_data (ctx);
    sqlite3_result_int64 (ctx, priv->last_account_id);
}

static void
create_functions (AgManagerPrivate *priv)
{
    sqlite3_create_function (priv->db, "set_last_rowid_as_service_id", 1,
                             SQLITE_ANY, priv,
                             set_last_rowid_as_service_id, NULL, NULL);
    sqlite3_create_function (priv->db, "set_last_rowid_as_account_id", 0,
                             SQLITE_ANY, priv,
                             set_last_rowid_as_account_id, NULL, NULL);
    sqlite3_create_function (priv->db, "service_id", 0,
                             SQLITE_ANY, priv,
                             get_service_id, NULL, NULL);
    sqlite3_create_function (priv->db, "account_id", 0,
                             SQLITE_ANY, priv,
                             get_account_id, NULL, NULL);
}

static gboolean
open_db (AgManager *manager)
{
    AgManagerPrivate *priv = manager->priv;
    const gchar *sql, *basedir;
    gchar *filename, *error;
    int ret;

    basedir = g_getenv ("ACCOUNTS");
    if (G_LIKELY (!basedir))
        basedir = g_get_home_dir ();
    filename = g_build_filename (basedir, "accounts.db", NULL);
    ret = sqlite3_open (filename, &priv->db);
    g_free (filename);

    if (ret != SQLITE_OK)
    {
        if (priv->db)
        {
            g_warning ("Error opening accounts DB: %s",
                       sqlite3_errmsg (priv->db));
            sqlite3_close (priv->db);
            priv->db = NULL;
        }
        return FALSE;
    }

    /* TODO: busy handler */

    sql = ""
        "CREATE TABLE IF NOT EXISTS Accounts ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT,"
            "provider TEXT,"
            "enabled INTEGER);"

        "CREATE TABLE IF NOT EXISTS Services ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL UNIQUE,"
            "display TEXT NOT NULL,"
            /* following fields are included for performance reasons */
            "provider TEXT,"
            "type TEXT);"
        "CREATE INDEX IF NOT EXISTS idx_service ON Services(name);"

        "CREATE TABLE IF NOT EXISTS Settings ("
            "account INTEGER NOT NULL,"
            "service INTEGER,"
            "key TEXT NOT NULL,"
            "type TEXT NOT NULL,"
            "value BLOB);"
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_setting ON Settings "
            "(account, service, key);"

        "CREATE TRIGGER IF NOT EXISTS tg_delete_account "
            "BEFORE DELETE ON Accounts FOR EACH ROW BEGIN "
                "DELETE FROM Services WHERE id = OLD.id; "
                "DELETE FROM Settings WHERE account = OLD.id; "
            "END;";

    error = NULL;
    ret = sqlite3_exec (priv->db, sql, NULL, NULL, &error);
    if (ret != SQLITE_OK)
    {
        g_warning ("Error initializing DB: %s", error);
        sqlite3_free (error);
        sqlite3_close (priv->db);
        priv->db = NULL;
        return FALSE;
    }

    create_functions (priv);

    return TRUE;
}

static gboolean
setup_dbus (AgManager *manager)
{
    AgManagerPrivate *priv = manager->priv;
    DBusError error;
    gboolean ret;

    dbus_error_init (&error);
    priv->dbus_conn = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (G_UNLIKELY (dbus_error_is_set (&error)))
    {
        g_warning ("Failed to get D-Bus connection (%s)", error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    ret = dbus_connection_add_filter (priv->dbus_conn,
                                      dbus_filter_callback,
                                      manager, NULL);
    if (G_UNLIKELY (!ret))
    {
        g_warning ("Failed to add dbus filter");
        return FALSE;
    }

    dbus_error_init (&error);
    dbus_bus_add_match (priv->dbus_conn,
                        "type='signal',interface='" AG_DBUS_IFACE "'",
                        &error);
    if (G_UNLIKELY (dbus_error_is_set (&error)))
    {
        g_warning ("Failed to add dbus filter (%s)", error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    dbus_connection_setup_with_g_main (priv->dbus_conn, NULL);
    return TRUE;
}

static void
ag_manager_init (AgManager *manager)
{
    AgManagerPrivate *priv;

    manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager, AG_TYPE_MANAGER,
                                                 AgManagerPrivate);
    priv = manager->priv;

    priv->services =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               NULL, (GDestroyNotify)ag_service_unref);
    priv->accounts =
        g_hash_table_new_full (NULL, NULL,
                               NULL, (GDestroyNotify)account_weak_unref);
}

static GObject *
ag_manager_constructor (GType type, guint n_params,
                        GObjectConstructParam *params)
{
    GObjectClass *object_class = (GObjectClass *)ag_manager_parent_class;
    AgManager *manager;
    GObject *object;

    object = object_class->constructor (type, n_params, params);

    g_return_val_if_fail (object != NULL, NULL);

    manager = AG_MANAGER (object);
    if (G_UNLIKELY (!open_db (manager) || !setup_dbus (manager)))
    {
        g_object_unref (object);
        return NULL;
    }

    return object;
}

static void
ag_manager_dispose (GObject *object)
{
    AgManagerPrivate *priv = AG_MANAGER_PRIV (object);

    if (priv->is_disposed) return;
    priv->is_disposed = TRUE;

    while (priv->locks)
    {
        store_cb_data_free (priv->locks->data);
        priv->locks = g_list_delete_link (priv->locks, priv->locks);
    }

    G_OBJECT_CLASS (ag_manager_parent_class)->finalize (object);
}

static void
ag_manager_finalize (GObject *object)
{
    AgManagerPrivate *priv = AG_MANAGER_PRIV (object);

    if (priv->dbus_conn)
    {
        dbus_connection_remove_filter (priv->dbus_conn, dbus_filter_callback,
                                       object);
        dbus_connection_unref (priv->dbus_conn);
    }

    while (priv->emitted_signals)
    {
        g_slice_free (EmittedSignalData, priv->emitted_signals->data);
        priv->emitted_signals = g_list_delete_link (priv->emitted_signals,
                                                    priv->emitted_signals);
    }

    if (priv->begin_stmt)
        sqlite3_finalize (priv->begin_stmt);
    if (priv->commit_stmt)
        sqlite3_finalize (priv->commit_stmt);
    if (priv->rollback_stmt)
        sqlite3_finalize (priv->rollback_stmt);

    if (priv->services)
        g_hash_table_unref (priv->services);

    if (priv->accounts)
        g_hash_table_unref (priv->accounts);

    if (priv->db)
    {
        if (sqlite3_close (priv->db) != SQLITE_OK)
            g_warning ("Failed to close database: %s",
                       sqlite3_errmsg (priv->db));
        priv->db = NULL;
    }

    G_OBJECT_CLASS (ag_manager_parent_class)->finalize (object);
}

static void
ag_manager_class_init (AgManagerClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (AgManagerPrivate));

    object_class->constructor = ag_manager_constructor;
    object_class->dispose = ag_manager_dispose;
    object_class->finalize = ag_manager_finalize;

    /**
     * AgManager::account-created:
     * @manager: the #AgManager.
     * @account_id: the #AgAccountId of the account that has been created.
     *
     * Emitted when a new account has been created; note that the account must
     * have been stored in the database: the signal is not emitted just in
     * response to ag_manager_create_account().
     */
    signals[ACCOUNT_CREATED] = g_signal_new ("account-created",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE,
        1, G_TYPE_UINT);

    /**
     * AgManager::account-deleted:
     * @manager: the #AgManager.
     * @account_id: the #AgAccountId of the account that has been deleted.
     *
     * Emitted when an account has been deleted.
     * This signal is redundant with AgAccount::deleted, but it's convenient to
     * provide full change notification to #AgManager.
     */
    signals[ACCOUNT_DELETED] = g_signal_new ("account-deleted",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE,
        1, G_TYPE_UINT);
}

/**
 * ag_manager_new:
 *
 * Returns: an instance of an #AgManager.
 */
AgManager *
ag_manager_new ()
{
    return g_object_new (AG_TYPE_MANAGER, NULL);
}

/**
 * ag_manager_list:
 * @manager: the #AgManager.
 *
 * Lists the accounts.
 *
 * Returns: a #GList of #AgAccountId representing the accounts. Must
 * be free'd with ag_manager_list_free().
 */
GList *
ag_manager_list (AgManager *manager)
{
    GList *list = NULL;
    const gchar *sql;

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    sql = "SELECT id FROM Accounts;";
    _ag_manager_exec_query (manager, (AgQueryCallback)add_id_to_list,
                            &list, sql);
    return list;
}

/**
 * ag_manager_list_by_service_type:
 * @manager: the #AgManager.
 *
 * Lists the accounts supporting the given service type.
 *
 * Returns: a #GList of #AgAccountId representing the accounts. Must
 * be free'd with ag_manager_list_free().
 */
GList *
ag_manager_list_by_service_type (AgManager *manager,
                                 const gchar *service_type)
{
    GList *list = NULL;
    char sql[512];

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    sqlite3_snprintf (sizeof (sql), sql,
                      "SELECT DISTINCT account FROM Settings "
                      "JOIN Services ON Settings.service = Services.id "
                      "WHERE Services.type = %Q;",
                      service_type);
    _ag_manager_exec_query (manager, (AgQueryCallback)add_id_to_list,
                            &list, sql);
    return list;
}

/**
 * ag_manager_list_free:
 * @list: a #GList returned from some #AgManager method.
 *
 * Frees the memory taken by a #GList allocated by #AgManager.
 */
void
ag_manager_list_free (GList *list)
{
    g_list_free (list);
}

/**
 * ag_manager_get_account:
 * @manager: the #AgManager.
 * @account_id: the #AgAccountId of the account.
 *
 * Instantiates the object representing the account identified by
 * @account_id.
 *
 * Returns: an #AgAccount, on which the client must call g_object_unref()
 * when it's done with it.
 */
AgAccount *
ag_manager_get_account (AgManager *manager, AgAccountId account_id)
{
    AgManagerPrivate *priv;
    AgAccount *account;

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (account_id != 0, NULL);
    priv = manager->priv;

    account = g_hash_table_lookup (priv->accounts,
                                   GUINT_TO_POINTER (account_id));
    if (account)
        return g_object_ref (account);

    /* the account is not loaded; do it now */
    account = g_object_new (AG_TYPE_ACCOUNT,
                            "manager", manager,
                            "id", account_id,
                            NULL);
    if (G_LIKELY (account))
    {
        g_object_weak_ref (G_OBJECT (account), account_weak_notify, manager);
        g_hash_table_insert (priv->accounts, GUINT_TO_POINTER (account_id),
                             account);
    }
    return account;
}

/**
 * ag_manager_create_account:
 * @manager: the #AgManager.
 * @provider_name: name of the provider of the account.
 *
 * Create a new account. The account is not stored in the database until
 * ag_account_store() has successfully returned; the @id field in the
 * #AgAccount structure is also not meant to be valid till the account has been
 * stored.
 *
 * Returns: a new #AgAccount.
 */
AgAccount *
ag_manager_create_account (AgManager *manager, const gchar *provider_name)
{
    AgAccount *account;

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);

    account = g_object_new (AG_TYPE_ACCOUNT,
                            "manager", manager,
                            "provider", provider_name,
                            NULL);
    return account;
}

/**
 * ag_manager_get_service:
 * @manager: the #AgManager.
 * @service_name: the name of the service.
 *
 * Loads the service identified by @service_name.
 *
 * Returns: an #AgService, which must be then free'd with ag_service_unref().
 */
AgService *
ag_manager_get_service (AgManager *manager, const gchar *service_name)
{
    AgManagerPrivate *priv;
    AgService *service;
    gchar *sql;
    gint rows;


    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (service_name != NULL, NULL);
    priv = manager->priv;

    service = g_hash_table_lookup (priv->services, service_name);
    if (service)
        return ag_service_ref (service);

    /* First, check if the service is in the DB */
    sql = sqlite3_mprintf ("SELECT id, display, provider, type "
                           "FROM Services WHERE name = %Q", service_name);
    rows = _ag_manager_exec_query (manager, (AgQueryCallback)got_service,
                                   &service, sql);
    sqlite3_free (sql);

    if (service)
    {
        /* the basic server data have been loaded from the DB; the service name
         * is still missing, though */
        service->name = g_strdup (service_name);
    }
    else
    {
        /* The service is not in the DB: it must be loaded */
        service = _ag_service_new_from_file (service_name);
    }

    if (G_UNLIKELY (!service)) return NULL;

    g_hash_table_insert (priv->services, service->name, service);
    return ag_service_ref (service);
}

/**
 * ag_manager_list_services:
 * @manager: the #AgManager.
 *
 * Gets a list of all the installed services.
 *
 * Returns: a list of #AgService, which must be then free'd with
 * ag_service_list_free().
 */
GList *
ag_manager_list_services (AgManager *manager)
{
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);

    return _ag_services_list (manager);
}

/**
 * ag_manager_list_services_by_type:
 * @manager: the #AgManager.
 * @service_type: the type of the service.
 *
 * Gets a list of all the installed services of type @service_type.
 *
 * Returns: a list of #AgService, which must be then free'd with
 * ag_service_list_free().
 */
GList *
ag_manager_list_services_by_type (AgManager *manager, const gchar *service_type)
{
    GList *all_services, *list;
    GList *services = NULL;

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (service_type != NULL, NULL);

    /* if we kept the DB Service table always up-to-date with all known
     * services, then we could just run a query over it. But while we are not,
     * it's simpler to implement the function by reusing the output from
     * ag_manager_list_services(). */
    all_services = ag_manager_list_services (manager);
    for (list = all_services; list != NULL; list = list->next)
    {
        AgService *service = list->data;

        if (service->type && strcmp (service->type, service_type) == 0)
        {
            services = g_list_prepend (services, service);
        }
        else
            ag_service_unref (service);
    }
    g_list_free (all_services);

    return services;
}

void
_ag_manager_exec_transaction (AgManager *manager, const gchar *sql,
                              AgAccountChanges *changes, AgAccount *account,
                              AgAccountStoreCb callback, gpointer user_data)
{
    AgManagerPrivate *priv = manager->priv;
    GError error;
    int ret;

    ret = prepare_transaction_statements (priv);
    if (G_UNLIKELY (ret != SQLITE_OK))
        goto db_error;

    ret = sqlite3_step (priv->begin_stmt);
    if (ret == SQLITE_BUSY)
    {
        if (callback)
        {
            StoreCbData *sd;

            sd = g_slice_new (StoreCbData);
            sd->manager = manager;
            sd->account = account;
            sd->changes = changes;
            sd->callback = callback;
            sd->user_data = user_data;
            sd->sql = g_strdup (sql);
            sd->id = g_idle_add ((GSourceFunc)exec_transaction_idle, sd);
            priv->locks = g_list_prepend (priv->locks, sd);
            g_object_weak_ref (G_OBJECT (account), lost_weak_ref, sd);
        }
        return;
    }

    if (ret != SQLITE_DONE)
        goto db_error;

    exec_transaction (manager, account, sql, changes, callback, user_data);
    return;

db_error:
    error.domain = AG_ERRORS;
    error.code = AG_ERROR_DB;
    error.message = g_strdup_printf ("Got error: %s (%d)",
                                     sqlite3_errmsg (priv->db), ret);
    transaction_completed (manager, account, changes,
                           callback, &error, user_data);
    g_free (error.message);
}

/* Executes an SQL statement, and optionally calls
 * the callback for every row of the result.
 * Returns the number of rows fetched.
 */
gint
_ag_manager_exec_query (AgManager *manager,
                        AgQueryCallback callback, gpointer user_data,
                        const gchar *sql)
{
    sqlite3 *db;
    int ret;
    sqlite3_stmt *stmt;
    time_t try_until;
    gint rows = 0;

    g_return_val_if_fail (AG_IS_MANAGER (manager), 0);
    db = manager->priv->db;

    g_return_val_if_fail (db != NULL, 0);

    ret = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK)
    {
        g_debug ("%s: can't compile SQL statement \"%s\": %s", G_STRFUNC, sql,
                 sqlite3_errmsg (db));
        return 0;
    }

    g_debug ("%s: about to run:\n%s", G_STRFUNC, sql);

    /* Set maximum time we're prepared to wait. Have to do it here also,
     *    * because SQLite doesn't guarantee running the busy handler. Thanks,
     *       * SQLite. */
    try_until = time (NULL) + MAX_SQLITE_BUSY_LOOP_TIME;

    do
    {
        ret = sqlite3_step (stmt);

        switch (ret)
        {
            case SQLITE_DONE:
                break;

            case SQLITE_ROW:
                if (callback == NULL || callback (stmt, user_data))
                {
                    rows++;
                }
                break;

            case SQLITE_BUSY:
                if (time (NULL) < try_until)
                {
                    /* If timeout was specified and table is locked,
                     * wait instead of executing default runtime
                     * error action. Otherwise, fall through to it. */
                    sched_yield ();
                    break;
                }

            default:
                g_debug ("%s: runtime error while executing \"%s\": %s",
                         G_STRFUNC, sql, sqlite3_errmsg (db));
                sqlite3_finalize (stmt);
                return rows;
        }
    } while (ret != SQLITE_DONE);

    sqlite3_finalize (stmt);

    return rows;
}

