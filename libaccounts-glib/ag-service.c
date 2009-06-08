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

#include "ag-service.h"

#include "ag-internals.h"

static gchar *
find_service_file (const gchar *service_id)
{
    const gchar * const *dirs;
    const gchar *dirname;
    const gchar *env_dirname;
    gchar *filename, *filepath;

    filename = g_strdup_printf ("%s.service", service_id);
    env_dirname = g_getenv ("AG_SERVICES");
    if (env_dirname)
    {
        filepath = g_build_filename (env_dirname, filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    dirname = g_get_user_data_dir ();
    if (G_LIKELY (dirname))
    {
        filepath = g_build_filename (dirname, "accounts/services",
                                              filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    dirs = g_get_system_data_dirs ();
    for (dirname = *dirs; dirname != NULL; dirs++, dirname = *dirs)
    {
        filepath = g_build_filename (dirname, "accounts/services",
                                              filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    filepath = NULL;
found:
    g_free (filename);
    return filepath;
}

AgService *
_ag_service_new (const gchar *name,
                 const gchar *type,
                 const gchar *provider,
                 gint id)
{
    AgService *service;

    g_return_val_if_fail (name != NULL, NULL);

    service = g_slice_new0 (AgService);
    service->ref_count = 1;
    service->name = g_strdup (name);
    service->type = g_strdup (type);
    service->provider = g_strdup (provider);
    service->id = id;

    return service;
}

AgService *
_ag_service_load_from_file (const gchar *service_name)
{
    gchar *filepath;

    g_return_val_if_fail (service_name != NULL, NULL);

    filepath = find_service_file (service_name);
    if (G_UNLIKELY (!filepath)) return NULL;

    /* TODO: really load from file */

    /* for now, just create a fake service */
    return _ag_service_new (service_name, "e-mail", "sso-team", 0);
}

/**
 * ag_service_get_name:
 * @service: the #AgService.
 *
 * Returns: the name of @service.
 */
const gchar *
ag_service_get_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    return service->name;
}

/**
 * ag_service_get_service_type:
 * @service: the #AgService.
 *
 * Returns: the type of @service.
 */
const gchar *
ag_service_get_service_type (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    return service->type;
}

/**
 * ag_service_get_provider:
 * @service: the #AgService.
 *
 * Returns: the name of the provider of @service.
 */
const gchar *
ag_service_get_provider (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    return service->provider;
}

/**
 * ag_service_ref:
 * @service: the #AgService.
 *
 * Adds a reference to @service.
 *
 * Returns: @service.
 */
AgService *
ag_service_ref (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    g_return_val_if_fail (service->ref_count > 0, NULL);

    g_debug ("Referencing service %s (%d)",
             service->name, service->ref_count);
    service->ref_count++;
    return service;
}

/**
 * ag_service_unref:
 * @service: the #AgService.
 *
 * Used to unreference the #AgService structure.
 */
void
ag_service_unref (AgService *service)
{
    g_return_if_fail (service != NULL);
    g_return_if_fail (service->ref_count > 0);

    g_debug ("Unreferencing service %s (%d)",
             service->name, service->ref_count);
    service->ref_count--;
    if (service->ref_count == 0)
    {
        g_free (service->name);
        g_free (service->display_name);
        g_free (service->type);
        g_free (service->provider);
        g_slice_free (AgService, service);
    }
}

