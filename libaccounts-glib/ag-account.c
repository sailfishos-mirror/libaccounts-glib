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
 * SECTION:ag-account
 * @title: AgAccount
 * @short_description: A representation of an account.
 *
 * An #AgAccount is an object which represents an account. It provides a
 * method for enabling/disabling the account and methods for editing the 
 * account settings.
 */

#include "ag-account.h"

#include "ag-internals.h"
#include "ag-marshal.h"
#include "ag-service.h"
#include "ag-util.h"

#define SERVICE_GLOBAL "global"

enum
{
    PROP_0,

    PROP_ID,
    PROP_MANAGER,
    PROP_PROVIDER,
};

enum
{
    ENABLED,
    DISPLAY_NAME_CHANGED,
    DELETED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _AgServiceSettings AgServiceChanges;

typedef struct _AgServiceSettings {
    AgService *service;
    GHashTable *settings;
} AgServiceSettings;

struct _AgAccountChanges {
    gboolean enabled;
    gchar *display_name;

    guint enabled_changed : 1;
    guint display_name_changed : 1;

    /* The keys of the table are service names, and the values are
     * AgServiceChanges structures */
    GHashTable *services;
};

struct _AgAccountPrivate {
    AgManager *manager;

    /* selected service */
    AgService *service;

    gchar *provider_name;
    gchar *display_name;

    /* cached settings: keys are service names, values are AgServiceSettings
     * structures.
     * It may be that not all services are loaded in this hash table. But if a
     * service is present, then all of its settings are.
     */
    GHashTable *services;

    AgAccountChanges *changes;

    guint enabled : 1;
};

G_DEFINE_TYPE (AgAccount, ag_account, G_TYPE_OBJECT);

#define AG_ACCOUNT_PRIV(obj) (AG_ACCOUNT(obj)->priv)

static gboolean
got_account_setting (sqlite3_stmt *stmt, GHashTable *settings)
{
    gchar *key;
    GValue *value;

    key = g_strdup ((gchar *)sqlite3_column_text (stmt, 0));
    g_return_val_if_fail (key != NULL, FALSE);

    value = _ag_value_from_db (stmt, 1, 2);

    g_hash_table_insert (settings, key, value);
    return TRUE;
}

static void
ag_service_settings_free (AgServiceSettings *ss)
{
    if (ss->service)
        ag_service_unref (ss->service);
    g_hash_table_unref (ss->settings);
    g_slice_free (AgServiceSettings, ss);
}

static AgServiceSettings *
get_service_settings (AgAccountPrivate *priv, AgService *service,
                      gboolean create)
{
    AgServiceSettings *ss;
    const gchar *service_name;

    if (G_UNLIKELY (!priv->services))
    {
        priv->services = g_hash_table_new_full
            (g_str_hash, g_str_equal,
             NULL, (GDestroyNotify) ag_service_settings_free);
    }

    service_name = service ? service->name : SERVICE_GLOBAL;
    ss = g_hash_table_lookup (priv->services, service_name);
    if (!ss && create)
    {
        ss = g_slice_new (AgServiceSettings);
        ss->service = service ? ag_service_ref (service) : NULL;
        ss->settings = g_hash_table_new_full
            (g_str_hash, g_str_equal,
             g_free, (GDestroyNotify)_ag_value_slice_free);
        g_hash_table_insert (priv->services, (gchar *)service_name, ss);
    }

    return ss;
}

static void
ag_service_changes_free (AgServiceChanges *sc)
{
    g_hash_table_unref (sc->settings);
    g_slice_free (AgServiceChanges, sc);
}

void
_ag_account_changes_free (AgAccountChanges *changes)
{
    if (G_LIKELY (changes))
    {
        g_free (changes->display_name);
        g_hash_table_unref (changes->services);
        g_slice_free (AgAccountChanges, changes);
    }
}

/*
 * _ag_account_done_changes:
 *
 * This function is called after a successful execution of a transaction, and
 * must update the account data as with the contents of the AgAccountChanges
 * structure.
 */
void
_ag_account_done_changes (AgAccount *account, AgAccountChanges *changes)
{
    AgAccountPrivate *priv = account->priv;

    if (!changes) return;

    if (changes->enabled_changed)
    {
        priv->enabled = changes->enabled;
        g_signal_emit (account, signals[ENABLED], 0,
                       NULL, priv->enabled);
    }

    if (changes->display_name_changed)
    {
        g_free (priv->display_name);
        priv->display_name = changes->display_name;
        changes->display_name = NULL; /* we don't want it to be freed,
                                         since we stole it */
        g_signal_emit (account, signals[DISPLAY_NAME_CHANGED], 0);
    }

    /* TODO: update settings */
}

static AgAccountChanges *
account_changes_get (AgAccountPrivate *priv)
{
    if (!priv->changes)
    {
        priv->changes = g_slice_new0 (AgAccountChanges);
        priv->changes->services =
            g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                   (GDestroyNotify)ag_service_changes_free);
    }

    return priv->changes;
}

static void
change_service_value (AgAccountPrivate *priv,
                      const gchar *key, const GValue *value)
{
    AgAccountChanges *changes;
    AgServiceChanges *sc;
    gchar *service_name;

    changes = account_changes_get (priv);

    service_name = priv->service ? priv->service->name : SERVICE_GLOBAL;
    sc = g_hash_table_lookup (changes->services, service_name);
    if (!sc)
    {
        sc = g_slice_new (AgServiceChanges);
        sc->service = priv->service;
        sc->settings = g_hash_table_new_full
            (g_str_hash, g_str_equal,
             g_free, (GDestroyNotify)_ag_value_slice_free);
        g_hash_table_insert (changes->services, service_name, sc);
    }

    g_hash_table_insert (sc->settings,
                         g_strdup (key), _ag_value_slice_dup (value));
}

static void
ag_account_init (AgAccount *account)
{
    account->priv = G_TYPE_INSTANCE_GET_PRIVATE (account, AG_TYPE_ACCOUNT,
                                                 AgAccountPrivate);
}

static gboolean
got_account (sqlite3_stmt *stmt, AgAccountPrivate *priv)
{
    g_assert (priv->display_name == NULL);
    g_assert (priv->provider_name == NULL);
    priv->display_name = g_strdup ((gchar *)sqlite3_column_text (stmt, 0));
    priv->provider_name = g_strdup ((gchar *)sqlite3_column_text (stmt, 1));
    priv->enabled = sqlite3_column_int (stmt, 2);
    return TRUE;
}

static gboolean
ag_account_load (AgAccount *account)
{
    AgAccountPrivate *priv = account->priv;
    gchar sql[128];
    gint rows;

    g_snprintf (sql, sizeof (sql),
                "SELECT name, provider, enabled "
                "FROM Accounts WHERE id = %u", account->id);
    rows = _ag_manager_exec_query (priv->manager,
                                   (AgQueryCallback)got_account, priv, sql);

    ag_account_select_service (account, NULL);

    return rows == 1;
}

static GObject *
ag_account_constructor (GType type, guint n_params,
                        GObjectConstructParam *params)
{
    GObjectClass *object_class = (GObjectClass *)ag_account_parent_class;
    GObject *object;
    AgAccount *account;

    object = object_class->constructor (type, n_params, params);
    g_return_val_if_fail (AG_IS_ACCOUNT (object), NULL);

    account = AG_ACCOUNT (object);
    if (account->id && !ag_account_load (account))
    {
        g_warning ("Unable to load account %u", account->id);
        g_object_unref (object);
        return NULL;
    }

    return object;
}

static void
ag_account_set_property (GObject *object, guint property_id,
                         const GValue *value, GParamSpec *pspec)
{
    AgAccount *account = AG_ACCOUNT (object);
    AgAccountPrivate *priv = account->priv;

    switch (property_id)
    {
    case PROP_ID:
        g_assert (account->id == 0);
        account->id = g_value_get_uint (value);
        break;
    case PROP_MANAGER:
        g_assert (priv->manager == NULL);
        priv->manager = g_value_dup_object (value);
        break;
    case PROP_PROVIDER:
        g_assert (priv->provider_name == NULL);
        priv->provider_name = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
ag_account_dispose (GObject *object)
{
    AgAccount *account = AG_ACCOUNT (object);
    AgAccountPrivate *priv = account->priv;

    if (priv->manager)
    {
        g_object_unref (priv->manager);
        priv->manager = NULL;
    }

    G_OBJECT_CLASS (ag_account_parent_class)->dispose (object);
}

static void
ag_account_finalize (GObject *object)
{
    AgAccountPrivate *priv = AG_ACCOUNT_PRIV (object);

    g_free (priv->provider_name);
    g_free (priv->display_name);

    if (priv->services)
        g_hash_table_unref (priv->services);

    if (priv->changes)
    {
        g_debug ("Finalizing account with uncommitted changes!");
        _ag_account_changes_free (priv->changes);
    }

    G_OBJECT_CLASS (ag_account_parent_class)->finalize (object);
}

static void
ag_account_class_init (AgAccountClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (AgAccountPrivate));

    object_class->constructor = ag_account_constructor;
    object_class->set_property = ag_account_set_property;
    object_class->dispose = ag_account_dispose;
    object_class->finalize = ag_account_finalize;

    g_object_class_install_property
        (object_class, PROP_ID,
         g_param_spec_uint ("id", "id", "id",
                            0, G_MAXUINT, 0,
                            G_PARAM_STATIC_STRINGS |
                            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property
        (object_class, PROP_MANAGER,
         g_param_spec_object ("manager", "manager", "manager",
                              AG_TYPE_MANAGER,
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property
        (object_class, PROP_PROVIDER,
         g_param_spec_string ("provider", "provider", "provider",
                              NULL,
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                              G_PARAM_STATIC_STRINGS));

    /**
     * AgAccount::enabled:
     * @account: the #AgAccount.
     * @service: the service which was enabled/disabled, or %NULL if the global
     * enabledness of the account changed.
     * @enabled: the new state of the @account.
     *
     * Emitted when the account "enabled" status was changed for one of its
     * services, or for the account globally.
     */
    signals[ENABLED] = g_signal_new ("enabled",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        ag_marshal_VOID__STRING_BOOLEAN,
        G_TYPE_NONE,
        2, G_TYPE_STRING, G_TYPE_BOOLEAN);

    /**
     * AgAccount::display-name-changed:
     * @account: the #AgAccount.
     *
     * Emitted when the account display name has changed.
     */
    signals[DISPLAY_NAME_CHANGED] = g_signal_new ("display-name-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE,
        0);

    /**
     * AgAccount::deleted:
     * @account: the #AgAccount.
     *
     * Emitted when the account has been deleted.
     */
    signals[DELETED] = g_signal_new ("deleted",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

}

/**
 * ag_account_supports_service:
 * @account: the #AgAccount.
 *
 * Returns: a #gboolean which tells whether @account supports the service type
 * @service_type. 
 */
gboolean
ag_account_supports_service (AgAccount *account, const gchar *service_type)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * ag_account_list_services:
 * @account: the #AgAccount.
 *
 * Returns: a #GList of #AgService items representing all the services
 * supported by this account. Must be free'd with g_list_free().
 */
GList *
ag_account_list_services (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_account_list_services_by_type:
 * @account: the #AgAccount.
 * @service_type: the service type which all the returned services should
 * provide.
 *
 * Returns: a #GList of #AgService items representing all the services
 * supported by this account which provide @service_type. Must be free'd with
 * g_list_free().
 */
GList *
ag_account_list_services_by_type (AgAccount *account,
                                  const gchar *service_type)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_account_get_manager:
 * @account: the #AgAccount.
 *
 * Returns: the #AccountManager.
 */
AgManager *
ag_account_get_manager (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    return account->priv->manager;
}

/**
 * ag_account_get_provider_name:
 * @account: the #AgAccount.
 *
 * Returns: the name of the provider of @account.
 */
const gchar *
ag_account_get_provider_name (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    return account->priv->provider_name;
}

/**
 * ag_account_get_display_name:
 * @account: the #AgAccount.
 *
 * Returns: the display name for @account.
 */
const gchar *
ag_account_get_display_name (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    return account->priv->display_name;
}

/**
 * ag_account_set_display_name:
 * @account: the #AgAccount.
 * @display_name: the display name to set.
 *
 * Changes the display name for @account to @display_name.
 */
void
ag_account_set_display_name (AgAccount *account, const gchar *display_name)
{
    AgAccountPrivate *priv;
    AgAccountChanges *changes;

    g_return_if_fail (AG_IS_ACCOUNT (account));
    priv = account->priv;
    changes = account_changes_get (priv);

    changes->display_name = g_strdup (display_name);
    changes->display_name_changed = TRUE;
}

/**
 * ag_account_select_service:
 * @account: the #AgAccount.
 * @service: the #AgService to select.
 *
 * Selects the configuration of service @service: from now on, all the
 * subsequent calls on the #AgAccount configuration will act on the @service.
 * If @service is %NULL, the global account configuration is selected.
 *
 * Note that if @account is being shared with other code one must take special
 * care to make sure the desired service is always selected.
 */
void
ag_account_select_service (AgAccount *account, AgService *service)
{
    AgAccountPrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT (account));
    priv = account->priv;

    priv->service = service;

    if ((service == NULL || service->id != 0) && account->id != 0 &&
        !get_service_settings (priv, service, FALSE))
    {
        AgServiceSettings *ss;
        guint service_id;
        gchar sql[128];

        /* the settings for this service are not yet loaded: do it now */
        ss = get_service_settings (priv, service, TRUE);

        service_id = (service != NULL) ? service->id : 0;
        g_snprintf (sql, sizeof (sql),
                    "SELECT key, type, value FROM Settings "
                    "WHERE account = %u AND service = %u",
                    account->id, service_id);
        _ag_manager_exec_query (priv->manager,
                                (AgQueryCallback)got_account_setting,
                                ss->settings, sql);
    }
}

/**
 * ag_account_get_selected_service:
 * @account: the #AgAccount.
 *
 * Returns: the selected service, or %NULL if no service is selected.
 */
AgService *
ag_account_get_selected_service (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    return account->priv->service;
}

/**
 * ag_account_get_enabled:
 * @account: the #AgAccount.
 *
 * Returns: a #gboolean which tells whether the selected service for @account is
 * enabled.
 */
gboolean
ag_account_get_enabled (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), FALSE);
    return account->priv->enabled;
}

/**
 * ag_account_set_enabled:
 * @account: the #AgAccount.
 * @enabled: whether @account should be enabled.
 *
 * Sets the "enabled" flag on the selected service for @account.
 */
void
ag_account_set_enabled (AgAccount *account, gboolean enabled)
{
    AgAccountPrivate *priv;
    AgAccountChanges *changes;

    g_return_if_fail (AG_IS_ACCOUNT (account));
    priv = account->priv;
    if (priv->service == NULL)
    {
        changes = account_changes_get (priv);

        changes->enabled = (enabled != FALSE);
        changes->enabled_changed = TRUE;
    }
    else
    {
        GValue value = { 0 };

        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, enabled);
        change_service_value (priv, "enabled", &value);
    }
}

/**
 * ag_account_delete:
 * @account: the #AgAccount.
 *
 * Deletes the account. Call ag_account_store() in order to record the change
 * in the storage.
 */
void
ag_account_delete (AgAccount *account)
{
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * ag_account_get_value:
 * @account: the #AgAccount.
 * @key: the name of the setting to retrieve.
 * @value: an initialized #GValue to receive the setting's value.
 * 
 * Gets the value of the configuration setting @key: @value must be a
 * #GValue initialized to the type of the setting.
 *
 * Returns: one of <type>#AgSettingSource</type>: %AG_SETTING_SOURCE_NONE if the setting is
 * not present, %AG_SETTING_SOURCE_ACCOUNT if the setting comes from the
 * account configuration, or %AG_SETTING_SOURCE_PROFILE if the value comes as
 * predefined in the profile.
 */
AgSettingSource
ag_account_get_value (AgAccount *account, const gchar *key,
                      GValue *value)
{
    AgAccountPrivate *priv;
    AgServiceSettings *ss;

    g_return_val_if_fail (AG_IS_ACCOUNT (account), AG_SETTING_SOURCE_NONE);
    priv = account->priv;

    ss = get_service_settings (priv, priv->service, FALSE);
    if (ss)
    {
        const GValue *val;

        val = g_hash_table_lookup (ss->settings, key);
        if (val)
        {
            if (G_VALUE_TYPE (val) == G_VALUE_TYPE (value))
                g_value_copy (val, value);
            else
                g_value_transform (val, value);
            return AG_SETTING_SOURCE_ACCOUNT;
        }
    }

    g_warning ("%s: getting of default settings not implemented", G_STRFUNC);
    return AG_SETTING_SOURCE_NONE;
}

/**
 * ag_account_set_value:
 * @account: the #AgAccount.
 * @key: the name of the setting to change.
 * @value: a #GValue holding the new setting's value.
 *
 * Sets the value of the configuration setting @key to the value @value.
 * If @value is %NULL, then the setting is unset.
 */
void
ag_account_set_value (AgAccount *account, const gchar *key,
                      const GValue *value)
{
    AgAccountPrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT (account));
    priv = account->priv;

    change_service_value (priv, key, value);
}

/**
 * ag_account_settings_iter_init:
 * @account: the #AgAccount.
 * @iter: an uninitialized #AgAccountSettingIter structure.
 * @key_prefix: enumerate only the settings whose key starts with @key_prefix.
 *
 * Initializes @iter to iterate over the account settings. If @key_prefix is
 * not %NULL, only keys whose names start with @key_prefix will be iterated
 * over.
 */
void
ag_account_settings_iter_init (AgAccount *account,
                               AgAccountSettingIter *iter,
                               const gchar *key_prefix)
{
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * ag_account_settings_iter_next:
 * @iter: an initialized #AgAccountSettingIter structure.
 * @key: a pointer to a string receiving the key name.
 * @value: a pointer to a pointer to a #GValue, to receive the key value.
 *
 * Iterates over the account keys. @iter must be an iterator previously
 * initialized with ag_account_settings_iter_init().
 *
 * Returns: %TRUE if @key and @value have been set, %FALSE if we there are no
 * more account settings to iterate over.
 */
gboolean
ag_account_settings_iter_next (AgAccountSettingIter *iter,
                               const gchar **key, const GValue **value)
{
    g_return_val_if_fail (iter != NULL, FALSE);
    g_return_val_if_fail (AG_IS_ACCOUNT (iter->account), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * AgAccountNotifyCb:
 * @account: the #AgAccount.
 * @key: the name of the key whose value has changed.
 * @user_data: the user data that was passed when installing this callback.
 *
 * This callback is invoked when the value of an account configuration setting
 * changes. If the callback was installed with ag_account_watch_key() then @key
 * is the name of the configuration setting which changed; if it was installed
 * with ag_account_watch_dir() then @key is the same key prefix that was used
 * when installing this callback.
 */

/**
 * ag_account_watch_key:
 * @account: the #AgAccount.
 * @key: the name of the key to watch.
 * @callback: a #AgAccountNotifyCb callback to be called.
 * @user_data: pointer to user data, to be passed to @callback.
 *
 * Installs a watch on @key: @callback will be invoked whenever the value of
 * @key changes (or the key is removed).
 *
 * Returns: a #AgAccountWatch, which can then be used to remove this watch.
 */
AgAccountWatch
ag_account_watch_key (AgAccount *account, const gchar *key,
                      AgAccountNotifyCb callback, gpointer user_data)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_account_watch_dir:
 * @account: the #AgAccount.
 * @key_prefix: the prefix of the keys to watch.
 * @callback: a #AgAccountNotifyCb callback to be called.
 * @user_data: pointer to user data, to be passed to @callback.
 *
 * Installs a watch on all the keys under @key_prefix: @callback will be
 * invoked whenever the value of any of these keys changes (or a key is
 * removed).
 *
 * Returns: a #AgAccountWatch, which can then be used to remove this watch.
 */
AgAccountWatch
ag_account_watch_dir (AgAccount *account, const gchar *key_prefix,
                      AgAccountNotifyCb callback, gpointer user_data)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_account_remove_watch:
 * @account: the #AgAccount.
 * @watch: the watch to remove.
 *
 * Removes the notification callback identified by @watch.
 */
void
ag_account_remove_watch (AgAccount *account, AgAccountWatch watch)
{
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * AgAccountStoreCb:
 * @account: the #AgAccount.
 * @error: a #GError, or %NULL.
 * @user_data: the user data that was passed to ag_account_store().
 *
 * This callback is invoked when storing the account settings is completed. If
 * @error is not %NULL, then some error occurred and the data has most likely
 * not been written.
 */

/**
 * ag_account_store:
 * @account: the #AgAccount.
 * @callback: function to be called when the settings have been written.
 * @user_data: pointer to user data, to be passed to @callback.
 *
 * Store the account settings which have been changed into the account
 * database, and invoke @callback when the operation has been completed.
 */
void
ag_account_store (AgAccount *account, AgAccountStoreCb callback,
                  gpointer user_data)
{
    AgAccountPrivate *priv;
    AgAccountChanges *changes;
    GString *sql;
    const gchar *account_id_str;

    g_return_if_fail (AG_IS_ACCOUNT (account));
    priv = account->priv;

    changes = priv->changes;
    priv->changes = NULL;

    sql = g_string_sized_new (512);
    if (account->id == 0)
    {
        _ag_string_append_printf
            (sql,
             "INSERT INTO Accounts (name, provider, enabled) "
             "VALUES (%Q, %Q, %d);",
             changes ? changes->display_name : NULL,
             priv->provider_name,
             changes ? changes->enabled : 0);

        g_string_append (sql, "SELECT set_last_rowid_as_account_id();");
        account_id_str = "account_id()";
    }
    else if (changes &&
             (changes->display_name_changed || changes->enabled_changed))
    {
        gboolean comma = FALSE;
        g_string_append (sql, "UPDATE Accounts SET ");
        if (changes->display_name_changed)
        {
            _ag_string_append_printf
                (sql, "name = %Q", changes->display_name);
            comma = TRUE;
        }

        if (changes->enabled_changed)
        {
            _ag_string_append_printf
                (sql, "%cenabled = %d", comma ? ',' : ' ', changes->enabled);
        }

        _ag_string_append_printf (sql, " WHERE id = %d;", account->id);
    }

    if (changes)
    {
        GHashTableIter i_services;
        gpointer ht_key, ht_value;

        g_hash_table_iter_init (&i_services, changes->services);
        while (g_hash_table_iter_next (&i_services, &ht_key, &ht_value))
        {
            AgServiceChanges *sc = ht_value;
            GHashTableIter i_settings;
            const gchar *service_id_str;
            gchar service_id_buffer[16];

            if (sc->service)
            {
                if (sc->service->id == 0)
                {
                    /* the service is not in the DB: create the record now */
                    _ag_string_append_printf (sql,
                                              "INSERT INTO Services "
                                              "(name, type) VALUES (%Q, %Q);",
                                              sc->service->name,
                                              sc->service->type);
                    _ag_string_append_printf
                        (sql, "SELECT set_last_rowid_as_service_id(%Q);",
                         sc->service->name);
                    service_id_str = "service_id()";
                }
                else
                {
                    g_snprintf (service_id_buffer, sizeof (service_id_buffer),
                                "%d", sc->service->id);
                    service_id_str = service_id_buffer;
                }
            }
            else
                service_id_str = "0";

            g_hash_table_iter_init (&i_settings, sc->settings);
            while (g_hash_table_iter_next (&i_settings, &ht_key, &ht_value))
            {
                const gchar *key = ht_key;
                const GValue *value = ht_value;

                if (value)
                {
                    const gchar *value_str, *type_str;

                    value_str = _ag_value_to_db (value);
                    type_str = _ag_type_from_g_type (G_VALUE_TYPE (value));
                    _ag_string_append_printf
                        (sql,
                         "INSERT OR REPLACE INTO Settings (account, service,"
                                                          "key, type, value) "
                         "VALUES (%s, %s, %Q, %Q, %Q);",
                         account_id_str, service_id_str, key,
                         type_str, value_str);
                }
                else if (account->id != 0)
                {
                    _ag_string_append_printf
                        (sql,
                         "DELETE FROM Settings WHERE "
                         "account = %d AND "
                         "service = %Q AND "
                         "key = %Q;",
                         account->id, service_id_str, key);
                }
            }
        }
    }

    _ag_manager_exec_transaction (priv->manager, sql->str, changes, account,
                                  callback, user_data);
    g_string_free (sql, TRUE);
}

