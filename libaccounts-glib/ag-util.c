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

