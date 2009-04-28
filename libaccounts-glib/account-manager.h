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

#ifndef _ACCOUNT_MANAGER_H_
#define _ACCOUNT_MANAGER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define ACCOUNT_TYPE_MANAGER             (account_manager_get_type ())
#define ACCOUNT_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_TYPE_MANAGER, AccountManager))
#define ACCOUNT_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ACCOUNT_TYPE_MANAGER, AccountManagerClass))
#define ACCOUNT_IS_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCOUNT_TYPE_MANAGER))
#define ACCOUNT_IS_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCOUNT_TYPE_MANAGER))
#define ACCOUNT_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCOUNT_TYPE_MANAGER, AccountManagerClass))

typedef struct _AccountManagerClass AccountManagerClass;
typedef struct _AccountManagerPrivate AccountManagerPrivate;
typedef struct _AccountManager AccountManager;

#include <libaccounts-glib/account-item.h>

struct _AccountManagerClass
{
    GObjectClass parent_class;
};

struct _AccountManager
{
    GObject parent_instance;

    /*< private >*/
    AccountManagerPrivate *priv;
};

GType account_manager_get_type (void) G_GNUC_CONST;

AccountManager *account_manager_new (void);

GList *account_manager_list (AccountManager *account_manager);
GList *account_manager_list_by_service_type (AccountManager *account_manager,
                                             const gchar *service_type);
void account_manager_list_free (GList *list);

AccountItem *account_manager_get_account (AccountManager *account_manager,
                                          const gchar *account_name);

G_END_DECLS

#endif /* _ACCOUNT_MANAGER_H_ */
