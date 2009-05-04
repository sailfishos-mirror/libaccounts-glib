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

#ifndef _AG_MANAGER_H_
#define _AG_MANAGER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define AG_TYPE_MANAGER             (ag_manager_get_type ())
#define AG_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), AG_TYPE_MANAGER, AgManager))
#define AG_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), AG_TYPE_MANAGER, AgManagerClass))
#define AG_IS_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AG_TYPE_MANAGER))
#define AG_IS_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), AG_TYPE_MANAGER))
#define AG_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), AG_TYPE_MANAGER, AgManagerClass))

typedef struct _AgManagerClass AgManagerClass;
typedef struct _AgManagerPrivate AgManagerPrivate;
typedef struct _AgManager AgManager;

#include <libaccounts-glib/ag-account.h>

struct _AgManagerClass
{
    GObjectClass parent_class;
};

struct _AgManager
{
    GObject parent_instance;

    /*< private >*/
    AgManagerPrivate *priv;
};

GType ag_manager_get_type (void) G_GNUC_CONST;

AgManager *ag_manager_new (void);

GList *ag_manager_list (AgManager *manager);
GList *ag_manager_list_by_service_type (AgManager *manager,
                                        const gchar *service_type);
void ag_manager_list_free (GList *list);

AgAccount *ag_manager_get_account (AgManager *manager,
                                   const gchar *account_name);

G_END_DECLS

#endif /* _AG_MANAGER_H_ */
