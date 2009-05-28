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

AgService *
_ag_service_new (const gchar *name,
                 const gchar *type,
                 const gchar *provider,
                 gint id)
{
    AgService *service;

    g_return_val_if_fail (name != NULL, NULL);

    service = g_slice_new0 (AgService);
    service->name = g_strdup (name);
    service->type = g_strdup (type);
    service->provider = g_strdup (provider);
    service->id = id;

    return service;
}

AgService *
_ag_service_load_from_file (const gchar *service_name)
{
    g_return_val_if_fail (service_name != NULL, NULL);

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
 * ag_service_free:
 * @service: the #AgService.
 *
 * Used to free the #AgService structure.
 */
void
ag_service_free (AgService *service)
{
    g_return_if_fail (service != NULL);
    g_free (service->name);
    g_free (service->type);
    g_free (service->provider);
    g_slice_free (AgService, service);
}

