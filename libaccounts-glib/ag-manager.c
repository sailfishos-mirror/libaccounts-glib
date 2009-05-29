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
            "value BLOB);"

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

    return TRUE;
}

static void
ag_manager_init (AgManager *manager)
{
    manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager, AG_TYPE_MANAGER,
                                                 AgManagerPrivate);
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
     * @account_name: the name of the account that has been created.
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
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE,
        1, G_TYPE_STRING);

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
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
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
 * Returns: an #AgService, which must be then free'd with ag_service_free().
 */
AgService *
ag_manager_get_service (AgManager *manager, const gchar *service_name)
{
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_return_val_if_fail (service_name != NULL, NULL);
    g_warning ("Not implemented");
    return NULL;
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

