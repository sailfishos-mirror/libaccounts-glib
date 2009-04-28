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
 * SECTION:account-item
 * @title: AccountItem
 * @short_description: A representation of an account.
 *
 * An #AccountItem is an object which represents an account. It provides a
 * method for enabling/disabling the account and methods for editing the 
 * account settings.
 */

#include "account-item.h"

enum
{
    PROP_0,

    PROP_ACCOUNT_MANAGER,
};

enum
{
    ENABLED,
    STATIC_CONF_CHANGED,
    SETTING_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _AccountItemPrivate {
};

G_DEFINE_TYPE (AccountItem, account_item, G_TYPE_OBJECT);

#define ACCOUNT_ITEM_PRIV(obj) (ACCOUNT_ITEM(obj)->priv)

static void
account_item_init (AccountItem *item)
{
    item->priv = G_TYPE_INSTANCE_GET_PRIVATE (item, ACCOUNT_TYPE_ITEM,
                                              AccountItemPrivate);
}

static void
account_item_finalize (GObject *object)
{
    AccountItem *item = ACCOUNT_ITEM (object);
    AccountItemPrivate *priv = item->priv;

    g_free (item->name);

    G_OBJECT_CLASS (account_item_parent_class)->finalize (object);
}

static void
account_item_class_init (AccountItemClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = account_item_finalize;


    /**
     * AccountItem::enabled:
     * @account: the #AccountItem.
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
     * AccountItem::static-conf-changed:
     * @account: the #AccountItem.
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
     * AccountItem::setting-changed:
     * @account: the #AccountItem.
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
 * account_item_supports_service:
 * @item: the #AccountItem.
 *
 * Returns: a #gboolean which tells whether @item supports the service type
 * @service_type. 
 */
gboolean
account_item_supports_service (AccountItem *item, const gchar *service_type)
{
    g_return_val_if_fail (ACCOUNT_IS_ITEM (item), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * account_item_get_manager:
 * @item: the #AccountItem.
 *
 * Returns: the #AccountManager.
 */
AccountManager *
account_item_get_manager (AccountItem *item)
{
    g_return_val_if_fail (ACCOUNT_IS_ITEM (item), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * account_item_conf_select_service:
 * @item: the #AccountItem.
 * @service: the name of the service.
 *
 * Selects the configuration of service @service: from now on, all the
 * subsequent calls on the #AccountItem configuration will act on the @service.
 * If @service is %NULL, the global account configuration is selected.
 *
 * Note that if @item is being shared with other code one must take special
 * care to make sure the desired service is always selected.
 */
void
account_item_conf_select_service (AccountItem *item, const gchar *service)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * account_item_conf_get_enabled:
 * @item: the #AccountItem.
 *
 * Returns: a #gboolean which tells whether the selected service for @item is
 * enabled.
 */
gboolean
account_item_conf_get_enabled (AccountItem *item)
{
    g_return_val_if_fail (ACCOUNT_IS_ITEM (item), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * account_item_conf_set_enabled:
 * @item: the #AccountItem.
 * @enabled: whether @item should be enabled.
 *
 * Sets the "enabled" flag on the selected service for @item.
 */
void
account_item_conf_set_enabled (AccountItem *item, gboolean enabled)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * account_item_conf_get_static:
 * @item: the #AccountItem.
 * @key: the name of the setting to retrieve.
 * @value: an initialized #GValue to receive the setting's value.
 * 
 * Gets the value of the static configuration setting @key: @value must be a
 * #GValue initialized to the type of the setting.
 *
 * Returns: %TRUE if the setting is set on the account, %FALSE if it comes from
 * the service default configuration.
 */
gboolean
account_item_conf_get_static (AccountItem *item, const gchar *key,
                              GValue *value)
{
    g_return_val_if_fail (ACCOUNT_IS_ITEM (item), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * account_item_conf_set_static:
 * @item: the #AccountItem.
 * @key: the name of the setting to change.
 * @value: a #GValue holding the new setting's value.
 *
 * Sets the value of the static configuration setting @key to the value @value.
 */
void
account_item_conf_set_static (AccountItem *item, const gchar *key,
                              const GValue *value)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * AccountItemConfDynamicReadyCb:
 * @item: the #AccountItem.
 * @user_data: the user data that was passed to account_item_conf_call_when_dynamic_ready().
 * @weak_object: the weakly-referenced #GObject.
 *
 * This callback is invoked when the dynamic configuration for @item can be
 * accessed.
 */

/**
 * account_item_conf_call_when_dynamic_ready:
 * @item: the #AccountItem.
 * @callback: function to be called when the account dynamic configuration is
 * ready to be accessed.
 * @user_data: pointer to user data, to be passed to @callback.
 * @destroy: a #GDestroyNotify to be called on @user_data after the callback
 * has been invoked or the operation been cancelled.
 * @weak_object: if not %NULL, a #GObject which will be weakly referenced; if
 * it is destroyed, this call will automatically be cancelled. Must be %NULL if
 * @callback is %NULL.
 *
 * Starts monitoring the dynamic configuration for the selected service for
 * @item. Once the settings are available for retrieval, @callback is invoked.
 */
void
account_item_conf_call_when_dynamic_ready
    (AccountItem *item, AccountItemConfDynamicReadyCb callback,
     gpointer user_data, GDestroyNotify destroy, GObject *weak_object)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * account_item_conf_get_dynamic:
 * @item: the #AccountItem.
 * @key: the name of the setting to retrieve.
 * @value: an initialized #GValue to receive the setting's value.
 * 
 * Gets the value of the dynamic configuration setting @key: @value must be a
 * #GValue initialized to the type of the setting.
 *
 * Returns: %TRUE if the setting is set on the account, %FALSE if it comes from
 * the service default configuration.
 */
gboolean
account_item_conf_get_dynamic (AccountItem *item, const gchar *key,
                               GValue *value)
{
    g_return_val_if_fail (ACCOUNT_IS_ITEM (item), FALSE);
    g_warning ("%s not implemented", G_STRFUNC);
    return FALSE;
}

/**
 * account_item_conf_set_dynamic:
 * @item: the #AccountItem.
 * @key: the name of the setting to change.
 * @value: a #GValue holding the new setting's value.
 *
 * Sets the value of the dynamic configuration setting @key to the value
 * @value for the selected service on @item. Since the dynamic account storage
 * might be provided by an asynchronous service, callers of this method must
 * not assume that the new value has already been set when this method returns.
 */
void
account_item_conf_set_dynamic (AccountItem *item, const gchar *key,
                               const GValue *value)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * account_item_conf_begin_edit:
 * @item: the #AccountItem.
 *
 * Start an editing session for the account static configuration on the
 * selected service. This function is used to reduce the number of
 * notifications emitted when many settings on the static configuration are
 * being changed: by grouping the changes in between of
 * account_item_conf_begin_edit() and account_item_conf_end_edit(), change
 * notification will happen only once, when editing is finished.
 *
 * If this function is called, one should make no assumption on whether the
 * settings are effectively stored until account_item_conf_end_edit() is
 * called.
 */
void
account_item_conf_begin_edit (AccountItem *item)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

/**
 * account_item_conf_end_edit:
 * @item: the #AccountItem.
 *
 * Ends an editing session end emits the "static-conf-changed" signal, if
 * needed.
 */
void
account_item_conf_begin_edit (AccountItem *item)
{
    g_return_if_fail (ACCOUNT_IS_ITEM (item));
    g_warning ("%s not implemented", G_STRFUNC);
}

