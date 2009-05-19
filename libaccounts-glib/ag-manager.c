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

#include <sqlite3.h>

enum
{
    ACCOUNT_CREATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _AgManagerPrivate {
    sqlite3 *db;
};

G_DEFINE_TYPE (AgManager, ag_manager, G_TYPE_OBJECT);

#define AG_MANAGER_PRIV(obj) (AG_MANAGER(obj)->priv)

static gboolean
open_db (AgManager *manager)
{
    AgManagerPrivate *priv = manager->priv;
    const gchar *sql;
    gchar *filename, *error;
    int ret;

    filename = g_build_filename (g_get_home_dir (), "accounts.db", NULL);
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
            "name TEXT NOT NULL,"
            "type TEXT," /* for performance reasons */
            "enabled INTEGER);"

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
ag_manager_finalize (GObject *object)
{
    AgManagerPrivate *priv = AG_MANAGER_PRIV (object);

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
 * ag_account_store() has successfully returned; the @name field in the
 * #AgAccount structure is also not meant to be valid till the account has ben
 * stored.
 *
 * Returns: a new #AgAccount.
 */
AgAccount *
ag_manager_create_account (AgManager *manager, const gchar *provider_name)
{
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_service_get_name:
 * @service: the #AgService.
 *
 * Returns: the name of @service.
 */
const gchar *
ag_service_get_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_service_get_service_type:
 * @service: the #AgService.
 *
 * Returns: the type of @service.
 */
const gchar *
ag_service_get_service_type (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_service_get_provider:
 * @service: the #AgService.
 *
 * Returns: the name of the provider of @service.
 */
const gchar *
ag_service_get_provider (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

