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

#include <sched.h>

#define MAX_SQLITE_BUSY_LOOP_TIME 2

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

/* Executes an SQL statement, and optionally calls
 * the callback for every row of the result. Returns TRUE
 * if statement was successfully executed, FALSE on error. */
gboolean
_ag_db_exec (sqlite3 *db, GFunc cb, gpointer user_data, const gchar *sql)
{
    int ret;
    sqlite3_stmt *stmt;
    time_t try_until;

    g_return_val_if_fail (db != NULL, FALSE);

    ret = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK)
    {
        g_debug ("%s: can't compile SQL statement \"%s\": %s", G_STRFUNC, sql,
                 sqlite3_errmsg (db));
        return FALSE;
    }

    /* Set maximum time we're prepared to wait. Have to do it here also,
     *    * because SQLite doesn't guarantee running the busy handler. Thanks,
     *       * SQLite. */
    try_until = time (NULL) + MAX_SQLITE_BUSY_LOOP_TIME;

    do
    {
        ret = sqlite3_step (stmt);

        switch (ret)
        {
            case SQLITE_DONE:
                break;

            case SQLITE_ROW:
                if (cb != NULL)
                    cb ((gpointer) stmt, user_data);
                break;

            case SQLITE_BUSY:
                if (time (NULL) < try_until)
                {
                    /* If timeout was specified and table is locked,
                     * wait instead of executing default runtime
                     * error action. Otherwise, fall through to it. */
                    sched_yield ();
                    break;
                }

            default:
                g_debug ("%s: runtime error while executing \"%s\": %s",
                         G_STRFUNC, sql, sqlite3_errmsg (db));
                sqlite3_finalize (stmt);
                return FALSE;
        }
    } while (ret != SQLITE_DONE);

    sqlite3_finalize (stmt);

    return TRUE;
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

