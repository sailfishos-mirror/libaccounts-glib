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

#ifndef _ACCOUNT_ITEM_H_
#define _ACCOUNT_ITEM_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define ACCOUNT_TYPE_ITEM             (account_item_get_type ())
#define ACCOUNT_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_TYPE_ITEM, AccountItem))
#define ACCOUNT_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ACCOUNT_TYPE_ITEM, AccountItemClass))
#define ACCOUNT_IS_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCOUNT_TYPE_ITEM))
#define ACCOUNT_IS_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCOUNT_TYPE_ITEM))
#define ACCOUNT_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCOUNT_TYPE_ITEM, AccountItemClass))

typedef struct _AccountItemClass AccountItemClass;
typedef struct _AccountItemPrivate AccountItemPrivate;
typedef struct _AccountItem AccountItem;

#include "account-manager.h"

struct _AccountItemClass
{
    GObjectClass parent_class;
};

struct _AccountItem
{
    GObject parent_instance;
    gchar *name;

    /*< private >*/
    AccountItemPrivate *priv;
};

GType account_item_get_type (void) G_GNUC_CONST;

gboolean account_item_supports_service (AccountItem *item,
                                        const gchar *service_type);
AccountManager *account_item_get_manager (AccountItem *item);

/* Account configuration */
void account_item_conf_select_service (AccountItem *item,
                                       const gchar *service);

gboolean account_item_conf_get_enabled (AccountItem *item);
void account_item_conf_set_enabled (AccountItem *item, gboolean enabled);

gboolean account_item_conf_get_static (AccountItem *item, const gchar *key,
                                       GValue *value);
void account_item_conf_set_static (AccountItem *item, const gchar *key,
                                   const GValue *value);

typedef void (*AccountItemConfDynamicReadyCb) (AccountItem *item,
                                               gpointer userdata,
                                               GObject *weak_object);
void account_item_conf_call_when_dynamic_ready
    (AccountItem *item, AccountItemConfDynamicReadyCb callback,
     gpointer userdata, GDestroyNotify destroy, GObject *weak_object);

gboolean account_item_conf_get_dynamic (AccountItem *item, const gchar *key,
                                        GValue *value);
void account_item_conf_set_dynamic (AccountItem *item, const gchar *key,
                                    const GValue *value);

void account_item_conf_begin_edit (AccountItem *item);
void account_item_conf_end_edit (AccountItem *item);

/* Signon */
/* TODO: depends on signon-glib */

G_END_DECLS

#endif /* _ACCOUNT_ITEM_H_ */
