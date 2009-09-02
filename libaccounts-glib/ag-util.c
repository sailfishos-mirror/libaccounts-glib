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
#include <stdlib.h>
#include <string.h>

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
    switch (type)
    {
    case G_TYPE_STRING:
        return DBUS_TYPE_STRING_AS_STRING;
    case G_TYPE_INT:
    case G_TYPE_CHAR:
        return DBUS_TYPE_INT32_AS_STRING;
    case G_TYPE_UINT:
        return DBUS_TYPE_UINT32_AS_STRING;
    case G_TYPE_BOOLEAN:
        return DBUS_TYPE_BOOLEAN_AS_STRING;
    case G_TYPE_UCHAR:
        return DBUS_TYPE_BYTE_AS_STRING;
    case G_TYPE_INT64:
        return DBUS_TYPE_INT64_AS_STRING;
    case G_TYPE_UINT64:
        return DBUS_TYPE_UINT64_AS_STRING;
    default:
        g_warning ("%s: unsupported type ``%s''", G_STRFUNC,
                   g_type_name (type));
        return NULL;
    }
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

gboolean
_ag_value_set_from_string (GValue *value, const gchar *string)
{
    GType type;
    char *endptr;

    type = G_VALUE_TYPE (value);
    g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);

    if (G_UNLIKELY (!string)) return FALSE;

    switch (type)
    {
    case G_TYPE_STRING:
        g_value_set_string (value, string);
        break;
    case G_TYPE_INT:
        {
            gint i;
            i = strtol (string, &endptr, 0);
            if (endptr && endptr[0] != '\0')
                return FALSE;
            g_value_set_int (value, i);
        }
        break;
    case G_TYPE_UINT:
        {
            guint u;
            u = strtoul (string, &endptr, 0);
            if (endptr && endptr[0] != '\0')
                return FALSE;
            g_value_set_uint (value, u);
        }
        break;
    case G_TYPE_BOOLEAN:
        if (string[0] == '1' ||
            strcmp (string, "true") == 0 ||
            strcmp (string, "True") == 0)
            g_value_set_boolean (value, TRUE);
        else if (string[0] == '0' ||
                 strcmp (string, "false") == 0 ||
                 strcmp (string, "False") == 0)
            g_value_set_boolean (value, FALSE);
        else
        {
            g_warning ("%s: Invalid boolean value: %s", G_STRFUNC, string);
            return FALSE;
        }
        break;
    default:
        g_warning ("%s: unsupported type ``%s''", G_STRFUNC,
                   g_type_name (type));
        return FALSE;
    }

    return TRUE;
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

static void
ag_value_append (DBusMessageIter *iter, const GValue *value)
{
    DBusMessageIter var;
    gboolean basic_type = FALSE;
    const void *val;
    const gchar *val_str;
    gint val_int;
    gint64 val_int64;
    guint val_uint;
    guint64 val_uint64;
    int dbus_type;
    gchar dbus_type_as_string[2];


    switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_STRING:
        dbus_type = DBUS_TYPE_STRING;
        val_str = g_value_get_string (value);
        val = &val_str;
        basic_type = TRUE;
        break;
    case G_TYPE_INT:
        dbus_type = DBUS_TYPE_INT32;
        val_int = g_value_get_int (value);
        val = &val_int;
        basic_type = TRUE;
        break;
    case G_TYPE_CHAR:
        dbus_type = DBUS_TYPE_INT32;
        val_int = g_value_get_char (value);
        val = &val_int;
        basic_type = TRUE;
        break;
    case G_TYPE_UINT:
        dbus_type = DBUS_TYPE_UINT32;
        val_uint = g_value_get_uint (value);
        val = &val_uint;
        basic_type = TRUE;
        break;
    case G_TYPE_BOOLEAN:
        dbus_type = DBUS_TYPE_BOOLEAN;
        val_int = g_value_get_boolean (value);
        val = &val_int;
        basic_type = TRUE;
        break;
    case G_TYPE_UCHAR:
        dbus_type = DBUS_TYPE_BYTE;
        val_uint = g_value_get_uchar (value);
        val = &val_uint;
        basic_type = TRUE;
        break;
    case G_TYPE_INT64:
        dbus_type = DBUS_TYPE_INT64;
        val_int64 = g_value_get_int64 (value);
        val = &val_int64;
        basic_type = TRUE;
        break;
    case G_TYPE_UINT64:
        dbus_type = DBUS_TYPE_UINT64;
        val_uint64 = g_value_get_uint64 (value);
        val = &val_uint64;
        basic_type = TRUE;
        break;
    default:
        g_warning ("%s: unsupported type ``%s''", G_STRFUNC,
                   G_VALUE_TYPE_NAME (value));
        break;
    }

    if (basic_type)
    {
        dbus_type_as_string[0] = dbus_type;
        dbus_type_as_string[1] = '\0';
        dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT,
                                          dbus_type_as_string, &var);
        dbus_message_iter_append_basic (&var, dbus_type, val);
        dbus_message_iter_close_container (iter, &var);
    }
}

void
_ag_iter_append_dict_entry (DBusMessageIter *iter, const gchar *key,
                            const GValue *value)
{
    DBusMessageIter args;

    dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY, NULL, &args);
    dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &key);

    ag_value_append (&args, value);
    dbus_message_iter_close_container (iter, &args);
}

