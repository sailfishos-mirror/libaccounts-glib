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

enum
{
    PROP_0,

    PROP_MANAGER,
};

enum
{
    ENABLED,
    STATIC_CONF_CHANGED,
    SETTING_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _AgAccountPrivate {
};

G_DEFINE_TYPE (AgAccount, ag_account, G_TYPE_OBJECT);

#define AG_ACCOUNT_PRIV(obj) (AG_ACCOUNT(obj)->priv)

static void
ag_account_init (AgAccount *account)
{
    account->priv = G_TYPE_INSTANCE_GET_PRIVATE (account, AG_TYPE_ACCOUNT,
                                                 AgAccountPrivate);
}

static void
ag_account_finalize (GObject *object)
{
    AgAccount *account = AG_ACCOUNT (object);
    AgAccountPrivate *priv = account->priv;

    g_free (account->name);

    G_OBJECT_CLASS (ag_account_parent_class)->finalize (object);
}

static void
ag_account_class_init (AgAccountClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = ag_account_finalize;


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
        account_marshal_VOID__STRING_BOOLEAN,
        G_TYPE_NONE,
        2, G_TYPE_STRING, G_TYPE_BOOLEAN);

    /**
     * AgAccount::static-conf-changed:
     * @account: the #AgAccount.
     * @service: the service whose configuration changed, or %NULL if the
     * global configuration of the account changed.
     *
     * Emitted when the account static configuration changed. If @service is
     * set, then the configuration for that service changed, otherwise the
     * change was in the account global configuration.
     */
    signals[STATIC_CONF_CHANGED] = g_signal_new ("static-conf-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE,
        1, G_TYPE_STRING);

    /**
     * AgAccount::setting-changed:
     * @account: the #AgAccount.
     * @service: the service whose setting changed, or %NULL if it was a global
     * setting on the account.
     * @key: the name of the setting which changed.
     * @value: a #GValue with the new value of the setting.
     *
     * Emitted when an account dynamic setting has changed. @service is
     * set if the setting belongs to a specific service, and is %NULL if the
     * setting is global for the account.
     */
    signals[SETTING_CHANGED] = g_signal_new ("setting-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE,
        3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
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
 * ag_account_get_manager:
 * @account: the #AgAccount.
 *
 * Returns: the #AccountManager.
 */
AgManager *
ag_account_get_manager (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * ag_account_select_service:
 * @account: the #AgAccount.
 * @service: the name of the service.
 *
 * Selects the configuration of service @service: from now on, all the
 * subsequent calls on the #AgAccount configuration will act on the @service.
 * If @service is %NULL, the global account configuration is selected.
 *
 * Note that if @account is being shared with other code one must take special
 * care to make sure the desired service is always selected.
 */
void
ag_account_select_service (AgAccount *account, const gchar *service)
{
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * ag_account_get_selected_service:
 * @account: the #AgAccount.
 *
 * Returns: the name of the selected service.
 */
const gchar *
ag_account_get_selected_service (AgAccount *account)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
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
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
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
 * Returns: one of #AgSettingSource: %AG_SETTING_SOURCE_NONE if the setting is
 * not present, %AG_SETTING_SOURCE_ACCOUNT if the setting comes from the
 * account configuration, or %AG_SETTING_SOURCE_PROFILE if the value comes as
 * predefined in the profile.
 */
AgSettingSource
ag_account_get_value (AgAccount *account, const gchar *key,
                      GValue *value)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), AG_SETTING_SOURCE_NONE);
    g_warning ("%s not implemented", G_STRFUNC);
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
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
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
    g_return_if_fail (AG_IS_ACCOUNT (account));
    g_warning ("%s not implemented", G_STRFUNC);
}

