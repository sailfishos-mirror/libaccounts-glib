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
 * SECTION:account-manager
 * @title: AccountManager
 * @short_description: The account manager object
 *
 * The #AccountManager is the main object in this library.
 */

#include "account-manager.h"


G_DEFINE_TYPE (AccountManager, account_manager, G_TYPE_OBJECT);

static void
account_manager_init (AccountManager *object)
{
}

static void
account_manager_finalize (GObject *object)
{
    G_OBJECT_CLASS (account_manager_parent_class)->finalize (object);
}

static void
account_manager_class_init (AccountManagerClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = account_manager_finalize;
}

/**
 * account_manager_new:
 *
 * Returns: an instance of an #AccountManager.
 */
AccountManager *
account_manager_new ()
{
    return g_object_new (ACCOUNT_TYPE_MANAGER, NULL);
}

/**
 * account_manager_list:
 * @account_manager: the #AccountManager.
 *
 * Lists the accounts.
 *
 * Returns: a #GList of constant strings representing the account names. Must
 * be free'd with account_manager_list_free().
 */
GList *
account_manager_list (AccountManager *account_manager)
{
    g_return_val_if_fail (ACCOUNT_IS_MANAGER (account_manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * account_manager_list_by_service_type:
 * @account_manager: the #AccountManager.
 *
 * Lists the accounts supporting the given service type.
 *
 * Returns: a #GList of constant strings representing the account names. Must
 * be free'd with account_manager_list_free().
 */
GList *
account_manager_list_by_service_type (AccountManager *account_manager,
                                      const gchar *service_type)
{
    g_return_val_if_fail (ACCOUNT_IS_MANAGER (account_manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

/**
 * account_manager_list_free:
 * @list: a #GList returned from some #AccountManager method.
 *
 * Frees the memory taken by a #GList allocated by #AccountManager.
 */
void
account_manager_list_free (GList *list)
{
    g_list_free (list);
}

/**
 * account_manager_get_account:
 * @account_manager: the #AccountManager.
 * @account_name: a string holding the account unique name.
 *
 * Instantiates the object representing the account identified by
 * @account_name.
 *
 * Returns: an #AccountItem, on which the client must call g_object_unref()
 * when it's done with it.
 */
AccountItem *
account_manager_get_account (AccountManager *account_manager,
                             const gchar *account_name)
{
    g_return_val_if_fail (ACCOUNT_IS_MANAGER (account_manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

