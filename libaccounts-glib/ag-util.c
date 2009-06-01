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

#include "ag-util.h"
#include "ag-errors.h"

#include <dbus/dbus.h>
#include <sched.h>
#include <stdio.h>

GString *
_ag_string_append_printf (GString *string, const gchar *format, ...)
{
    va_list ap;
    char *sql;

    va_start (ap, format);
    sql = sqlite3_vmprintf (format, ap);
    va_end (ap);

    if (sql)
    {
        g_string_append (string, sql);
        sqlite3_free (sql);
    }

    return string;
}

GValue *
_ag_value_slice_dup (const GValue *value)
{
    GValue *copy;

    if (!value) return NULL;
    copy = g_slice_new0 (GValue);
    g_value_init (copy, G_VALUE_TYPE (value));
    g_value_copy (value, copy);
    return copy;
}

void
_ag_value_slice_free (GValue *value)
{
    g_value_unset (value);
    g_slice_free (GValue, value);
}

const gchar *
_ag_value_to_db (const GValue *value)
{
    static gchar buffer[32];
    GType type;

    g_return_val_if_fail (value != NULL, NULL);

    type = G_VALUE_TYPE (value);

    if (type == G_TYPE_STRING)
        return g_value_get_string (value);

    if (type == G_TYPE_UCHAR ||
        type == G_TYPE_UINT ||
        type == G_TYPE_UINT64 ||
        type == G_TYPE_FLAGS)
    {
        guint64 n;

        if (type == G_TYPE_UCHAR) n = g_value_get_uchar (value);
        else if (type == G_TYPE_UINT) n = g_value_get_uint (value);
        else if (type == G_TYPE_UINT64) n = g_value_get_uint64 (value);
        else if (type == G_TYPE_FLAGS) n = g_value_get_flags (value);
        else g_assert_not_reached ();

        snprintf (buffer, sizeof (buffer), "%llu", n);
        return buffer;
    }

    if (type == G_TYPE_CHAR ||
        type == G_TYPE_INT ||
        type == G_TYPE_INT64 ||
        type == G_TYPE_ENUM ||
        type == G_TYPE_BOOLEAN)
    {
        gint64 n;

        if (type == G_TYPE_CHAR) n = g_value_get_char (value);
        else if (type == G_TYPE_INT) n = g_value_get_int (value);
        else if (type == G_TYPE_INT64) n = g_value_get_int64 (value);
        else if (type == G_TYPE_ENUM) n = g_value_get_enum (value);
        else if (type == G_TYPE_BOOLEAN) n = g_value_get_boolean (value);
        else g_assert_not_reached ();

        snprintf (buffer, sizeof (buffer), "%lld", n);
        return buffer;
    }

    g_warning ("%s: unsupported type ``%s''", G_STRFUNC, g_type_name (type));
    return NULL;
}

const gchar *
_ag_type_from_g_type (GType type)
{
    if (type == G_TYPE_STRING)
        return DBUS_TYPE_STRING_AS_STRING;
    if (type == G_TYPE_INT ||
        type == G_TYPE_CHAR)
        return DBUS_TYPE_INT32_AS_STRING;
    if (type == G_TYPE_UINT)
        return DBUS_TYPE_UINT32_AS_STRING;
    if (type == G_TYPE_BOOLEAN)
        return DBUS_TYPE_BOOLEAN_AS_STRING;
    if (type == G_TYPE_UCHAR)
        return DBUS_TYPE_BYTE_AS_STRING;
    if (type == G_TYPE_INT64)
        return DBUS_TYPE_INT64_AS_STRING;
    if (type == G_TYPE_UINT64)
        return DBUS_TYPE_UINT64_AS_STRING;

    g_warning ("%s: unsupported type ``%s''", G_STRFUNC, g_type_name (type));
    return NULL;
}

GType
_ag_type_to_g_type (const gchar *type_str)
{
    g_return_val_if_fail (type_str != NULL, G_TYPE_INVALID);

    switch (type_str[0])
    {
    case DBUS_TYPE_STRING:
        return G_TYPE_STRING;
    case DBUS_TYPE_INT32:
        return G_TYPE_INT;
    case DBUS_TYPE_UINT32:
        return G_TYPE_UINT;
    case DBUS_TYPE_BOOLEAN:
        return G_TYPE_BOOLEAN;
    case DBUS_TYPE_BYTE:
        return G_TYPE_UCHAR;
    default:
        g_warning ("%s: unsupported type ``%s''", G_STRFUNC, type_str);
        return G_TYPE_INVALID;
    }
}

GValue *
_ag_value_from_db (sqlite3_stmt *stmt, gint col_type, gint col_value)
{
    GValue *value;
    GType type;

    type = _ag_type_to_g_type ((gchar *) sqlite3_column_text (stmt, col_type));
    g_return_val_if_fail (type != G_TYPE_INVALID, NULL);

    value = g_slice_new0 (GValue);
    g_value_init (value, type);

    switch (type)
    {
    case G_TYPE_STRING:
        g_value_set_string (value,
                            (gchar *)sqlite3_column_text (stmt, col_value));
        break;
    case G_TYPE_INT:
        g_value_set_int (value, sqlite3_column_int (stmt, col_value));
        break;
    case G_TYPE_UINT:
        g_value_set_uint (value, sqlite3_column_int64 (stmt, col_value));
        break;
    case G_TYPE_BOOLEAN:
        g_value_set_boolean (value, sqlite3_column_int (stmt, col_value));
        break;
    case G_TYPE_INT64:
        g_value_set_int64 (value, sqlite3_column_int64 (stmt, col_value));
        break;
    case G_TYPE_UINT64:
        g_value_set_uint64 (value, sqlite3_column_int64 (stmt, col_value));
        break;
    default:
        g_warning ("%s: unsupported type ``%s''", G_STRFUNC,
                   g_type_name (type));
        _ag_value_slice_free (value);
        return NULL;
    }

    return value;
}

/**
 * ag_errors_quark:
 *
 * Return the libaccounts-glib error domain.
 *
 * Returns: the libaccounts-glib error domain.
 */
GQuark
ag_errors_quark (void)
{
    static gsize quark = 0;

    if (g_once_init_enter (&quark))
    {
        GQuark domain = g_quark_from_static_string ("ag_errors");

        g_assert (sizeof (GQuark) <= sizeof (gsize));

        g_once_init_leave (&quark, domain);
    }

    return (GQuark) quark;
}

