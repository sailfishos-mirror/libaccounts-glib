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

#ifndef _AG_UTIL_H_
#define _AG_UTIL_H_

#include <glib.h>
#include <glib-object.h>
#include <sqlite3.h>

G_BEGIN_DECLS

GString *_ag_string_append_printf (GString *string,
                                   const gchar *format,
                                   ...) G_GNUC_INTERNAL;

G_GNUC_INTERNAL
GValue *_ag_value_slice_dup (const GValue *value);

G_GNUC_INTERNAL
void _ag_value_slice_free (GValue *value);

G_GNUC_INTERNAL
const gchar *_ag_value_to_db (const GValue *value);

G_GNUC_INTERNAL
GValue *_ag_value_from_db (sqlite3_stmt *stmt, gint col_type, gint col_value);

G_GNUC_INTERNAL
const gchar *_ag_type_from_g_type (GType type);

G_GNUC_INTERNAL
GType _ag_type_to_g_type (const gchar *type_str);

G_GNUC_INTERNAL
gboolean _ag_value_set_from_string (GValue *value, const gchar *string);

G_END_DECLS

#endif /* _AG_UTIL_H_ */
