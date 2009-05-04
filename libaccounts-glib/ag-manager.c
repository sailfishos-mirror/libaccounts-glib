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


G_DEFINE_TYPE (AgManager, ag_manager, G_TYPE_OBJECT);

static void
ag_manager_init (AgManager *object)
{
}

static void
ag_manager_finalize (GObject *object)
{
    G_OBJECT_CLASS (ag_manager_parent_class)->finalize (object);
}

static void
ag_manager_class_init (AgManagerClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = ag_manager_finalize;
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
 * Returns: a #GList of constant strings representing the account names. Must
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
 * Returns: a #GList of constant strings representing the account names. Must
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
 * @account_name: a string holding the account unique name.
 *
 * Instantiates the object representing the account identified by
 * @account_name.
 *
 * Returns: an #AgAccount, on which the client must call g_object_unref()
 * when it's done with it.
 */
AgAccount *
ag_manager_get_account (AgManager *manager,
                        const gchar *account_name)
{
    g_return_val_if_fail (AG_IS_MANAGER (manager), NULL);
    g_warning ("%s not implemented", G_STRFUNC);
    return NULL;
}

