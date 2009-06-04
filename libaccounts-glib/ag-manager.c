/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
 *
 * This software, including documentation, is protected by copyright controlled
 * by Nokia Corporation. All rights are reserved.
 * Copying, including reproducing, storing, adapting or translating, any or all
 * of this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
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
#include <sched.h>
#include <sqlite3.h>

#define MAX_SQLITE_BUSY_LOOP_TIME 2

enum
{
    ACCOUNT_CREATED,
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

    /* Cache for AgService */
    GHashTable *services;

    /* list of StoreCbData awaiting for exclusive locks */
    GList *locks;

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

G_DEFINE_TYPE (AgManager, ag_manager, G_TYPE_OBJECT);

#define AG_MANAGER_PRIV(obj) (AG_MANAGER(obj)->priv)

static void store_cb_data_free (StoreCbData *sd);

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
        account->id = priv->last_account_id;

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
            "type TEXT);" /* for performance reasons */
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
}

static GObject *
ag_manager_constructor (GType type, guint n_params,
                        GObjectConstructParam *params)
{
    GObjectClass *object_class = (GObjectClass *)ag_manager_parent_class;
    GObject *object;

    object = object_class->constructor (type, n_params, params);

    g_return_val_if_fail (object != NULL, NULL);

    if (G_UNLIKELY (!open_db (AG_MANAGER (object))))
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

    if (priv->begin_stmt)
        sqlite3_finalize (priv->begin_stmt);
    if (priv->commit_stmt)
        sqlite3_finalize (priv->commit_stmt);
    if (priv->rollback_stmt)
        sqlite3_finalize (priv->rollback_stmt);

    if (priv->services)
        g_hash_table_unref (priv->services);

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
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
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
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
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
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (account_id != 0, NULL);
    return g_object_new (AG_TYPE_ACCOUNT,
                         "manager", manager,
                         "id", account_id,
                         NULL);
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

    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (service_name != NULL, NULL);
    priv = manager->priv;

    service = g_hash_table_lookup (priv->services, service_name);
    if (service)
        return ag_service_ref (service);

    /* TODO first, check if the service is in the DB */

    /* The service is not in the DB: it must be loaded */
    service = _ag_service_load_from_file (service_name);

    g_hash_table_insert (priv->services, service->name, service);
    return ag_service_ref (service);
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

