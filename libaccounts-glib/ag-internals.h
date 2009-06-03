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

#ifndef _AG_INTERNALS_H_
#define _AG_INTERNALS_H_

#include "ag-manager.h"
#include <sqlite3.h>

G_BEGIN_DECLS

typedef struct _AgAccountChanges AgAccountChanges;

void _ag_account_changes_free (AgAccountChanges *changes) G_GNUC_INTERNAL;

G_GNUC_INTERNAL
void _ag_account_done_changes (AgAccount *account, AgAccountChanges *changes);

void _ag_manager_exec_transaction (AgManager *manager, const gchar *sql,
                                   AgAccountChanges *changes,
                                   AgAccount *account,
                                   AgAccountStoreCb callback,
                                   gpointer user_data) G_GNUC_INTERNAL;

typedef gboolean (*AgQueryCallback) (sqlite3_stmt *stmt, gpointer user_data);

G_GNUC_INTERNAL
gint _ag_manager_exec_query (AgManager *manager,
                             AgQueryCallback callback, gpointer user_data,
                             const gchar *sql);

struct _AgService {
    gint ref_count;
    gchar *name;
    gchar *type;
    gchar *provider;
    gint id;
};

G_GNUC_INTERNAL
AgService *_ag_service_load_from_file (const gchar *service_name);

G_GNUC_INTERNAL
AgService *_ag_service_new (const gchar *name,
                            const gchar *type,
                            const gchar *provider,
                            gint id);

G_END_DECLS

#endif /* _AG_INTERNALS_H_ */
