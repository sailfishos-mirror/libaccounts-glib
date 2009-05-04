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

#ifndef _AG_ACCOUNT_H_
#define _AG_ACCOUNT_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define AG_TYPE_ACCOUNT             (ag_account_get_type ())
#define AG_ACCOUNT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), AG_TYPE_ACCOUNT, AgAccount))
#define AG_ACCOUNT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), AG_TYPE_ACCOUNT, AgAccountClass))
#define AG_IS_ACCOUNT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AG_TYPE_ACCOUNT))
#define AG_IS_ACCOUNT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), AG_TYPE_ACCOUNT))
#define AG_ACCOUNT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), AG_TYPE_ACCOUNT, AgAccountClass))

typedef struct _AgAccountClass AgAccountClass;
typedef struct _AgAccountPrivate AgAccountPrivate;
typedef struct _AgAccount AgAccount;

#include "ag-manager.h"

struct _AgAccountClass
{
    GObjectClass parent_class;
};

struct _AgAccount
{
    GObject parent_instance;
    gchar *name;

    /*< private >*/
    AgAccountPrivate *priv;
};

GType ag_account_get_type (void) G_GNUC_CONST;

gboolean ag_account_supports_service (AgAccount *account,
                                      const gchar *service_type);
AgManager *ag_account_get_manager (AgAccount *account);

/* Account configuration */
void ag_account_select_service (AgAccount *account,
                                const gchar *service);

gboolean ag_account_get_enabled (AgAccount *account);
void ag_account_set_enabled (AgAccount *account, gboolean enabled);

typedef enum {
    AG_SETTING_SOURCE_NONE = 0,
    AG_SETTING_SOURCE_ACCOUNT,
    AG_SETTING_SOURCE_PROFILE,
} AgSettingSource;

AgSettingSource ag_account_get_value (AgAccount *account, const gchar *key,
                                      GValue *value);
void ag_account_set_value (AgAccount *account, const gchar *key,
                           const GValue *value);

typedef struct _AgAccountSettingIter AgAccountSettingIter;

struct _AgAccountSettingIter {
    AgAccount *account;
    /*< private >*/
    gpointer ptr1;
    gpointer ptr2;
    gint idx1;
    gint idx2;
};

void ag_account_settings_iter_init (AgAccount *account,
                                    AgAccountSettingIter *iter,
                                    const gchar *key_prefix);
gboolean ag_account_settings_iter_next (AgAccountSettingIter *iter,
                                        const gchar **key,
                                        const GValue *value);

typedef void (AgAccountStoreCb) (AgAccount *account, const GError *error,
                                 gpointer user_data);
void ag_account_store (AgAccount *account, AgAccountStoreCb callback,
                       gpointer user_data);

/* Signon */
/* TODO: depends on signon-glib */

G_END_DECLS

#endif /* _AG_ACCOUNT_H_ */
