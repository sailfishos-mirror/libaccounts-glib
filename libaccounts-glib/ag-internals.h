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

G_BEGIN_DECLS

void _ag_manager_exec_transaction (AgManager *manager, const gchar *sql,
                                   AgAccount *account,
                                   AgAccountStoreCb callback,
                                   gpointer user_data) G_GNUC_INTERNAL;

G_END_DECLS

#endif /* _AG_INTERNALS_H_ */
