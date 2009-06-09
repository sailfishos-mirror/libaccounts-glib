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
#include "ag-util.h"
#include <libxml/xmlreader.h>
#include <string.h>


static gboolean
get_element_data (xmlTextReaderPtr reader, const gchar **dest_ptr)
{
    if (dest_ptr) *dest_ptr = NULL;

    if (xmlTextReaderIsEmptyElement (reader))
        return TRUE;

    if (xmlTextReaderRead (reader) != 1 ||
        xmlTextReaderNodeType (reader) != XML_READER_TYPE_TEXT)
        return FALSE;

    if (dest_ptr)
        *dest_ptr = (const gchar *)xmlTextReaderConstValue (reader);

    return TRUE;
}

static gboolean
close_element (xmlTextReaderPtr reader)
{
    if (xmlTextReaderRead (reader) != 1 ||
        xmlTextReaderNodeType (reader) != XML_READER_TYPE_END_ELEMENT)
        return FALSE;

    return TRUE;
}

static gboolean
dup_element_data (xmlTextReaderPtr reader, gchar **dest_ptr)
{
    const gchar *data;
    gboolean ret;

    ret = get_element_data (reader, &data);
    if (dest_ptr)
        *dest_ptr = g_strdup (data);

    close_element (reader);
    return ret;
}

static gboolean
parse_param (xmlTextReaderPtr reader, GValue *value)
{
    const gchar *name, *str_value, *str_type;
    gboolean ok;
    GType type;

    name = (const gchar *)xmlTextReaderGetAttribute (reader,
                                                     (xmlChar *) "name");

    str_type = (const gchar *)xmlTextReaderGetAttribute (reader,
                                                         (xmlChar *) "type");
    if (!str_type)
        str_type = "s"; /* string */

    ok = get_element_data (reader, &str_value);
    if (G_UNLIKELY (!ok)) return FALSE;

    g_debug ("Default param %s, value %s", name, str_value);
    type = _ag_type_to_g_type (str_type);
    if (G_UNLIKELY (type == G_TYPE_INVALID)) return FALSE;

    g_value_init (value, type);

    ok = _ag_value_set_from_string (value, str_value);
    if (G_UNLIKELY (!ok)) return FALSE;

    ok = close_element (reader);
    if (G_UNLIKELY (!ok)) return FALSE;

    return TRUE;
}

static gboolean
parse_settings (xmlTextReaderPtr reader, const gchar *group,
                GHashTable *settings)
{
    const gchar *name;
    int ret, type;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            g_debug ("found name %s", name);
            if (strcmp (name, "setting") == 0)
            {
                GValue value = { 0 }, *pval;
                const gchar *key_name;
                gchar *key;

                key_name = (const gchar *)xmlTextReaderGetAttribute
                    (reader, (xmlChar *)"name");
                key = g_strdup_printf ("%s%s", group, key_name);

                ok = parse_param (reader, &value);
                if (ok)
                {
                    pval = g_slice_new0 (GValue);
                    g_value_init (pval, G_VALUE_TYPE (&value));
                    g_value_copy (&value, pval);

                    g_hash_table_insert (settings, key, pval);
                }
                else
                    g_free (key);
            }
            else
            {
                /* it's a subgroup */
                if (!xmlTextReaderIsEmptyElement (reader))
                {
                    gchar *subgroup;

                    subgroup = g_strdup_printf ("%s%s/", group, name);
                    ok = parse_settings (reader, subgroup, settings);
                    g_free (subgroup);
                }
                else
                    ok = TRUE;
            }

            if (G_UNLIKELY (!ok)) return FALSE;
        }

        ret = xmlTextReaderNext (reader);
    }
    return TRUE;
}

static gboolean
parse_template (xmlTextReaderPtr reader, AgService *service)
{
    GHashTable *settings;
    gboolean ok;

    g_return_val_if_fail (service->default_settings == NULL, FALSE);

    settings =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, (GDestroyNotify)_ag_value_slice_free);

    ok = parse_settings (reader, "", settings);
    if (G_UNLIKELY (!ok))
    {
        g_hash_table_destroy (settings);
        return FALSE;
    }

    service->default_settings = settings;
    return TRUE;
}

static gboolean
parse_preview (xmlTextReaderPtr reader, AgService *service)
{
    /* TODO: implement */
    return TRUE;
}

static gboolean
parse_service (xmlTextReaderPtr reader, AgService *service)
{
    const gchar *name;
    int ret, type;

    service->name = g_strdup
        ((const gchar *)xmlTextReaderGetAttribute (reader, (xmlChar *) "id"));

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "service") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "type") == 0)
            {
                ok = dup_element_data (reader, &service->type);
            }
            else if (strcmp (name, "name") == 0)
            {
                ok = dup_element_data (reader, &service->display_name);
            }
            else if (strcmp (name, "icon") == 0)
            {
                /* TODO: Store icon name somewhere */
                ok = get_element_data (reader, NULL);
            }
            else if (strcmp (name, "template") == 0)
            {
                ok = parse_template (reader, service);
            }
            else if (strcmp (name, "preview") == 0)
            {
                ok = parse_preview (reader, service);
            }
            else
                ok = TRUE;

            if (G_UNLIKELY (!ok)) return FALSE;
        }

        ret = xmlTextReaderNext (reader);
    }
    return TRUE;
}

static gboolean
read_service_file (xmlTextReaderPtr reader, AgService *service)
{
    const xmlChar *name;
    int ret;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = xmlTextReaderConstName (reader);
        if (G_LIKELY (name &&
                      strcmp ((const gchar *)name, "service") == 0))
        {
            return parse_service (reader, service);
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

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
_ag_service_new (void)
{
    AgService *service;

    service = g_slice_new0 (AgService);
    service->ref_count = 1;

    return service;
}

AgService *
_ag_service_load_from_file (const gchar *service_name)
{
    xmlTextReaderPtr reader;
    AgService *service;
    gchar *filepath;

    g_return_val_if_fail (service_name != NULL, NULL);

    filepath = find_service_file (service_name);
    if (G_UNLIKELY (!filepath)) return NULL;

    /* TODO: cache the xmlReader */
    reader = xmlReaderForFile (filepath, NULL, 0);
    g_free (filepath);

    if (G_UNLIKELY (reader == NULL))
        return NULL;

    service = _ag_service_new ();
    if (!read_service_file (reader, service))
    {
        ag_service_unref (service);
        service = NULL;
    }

    xmlFreeTextReader (reader);
    return service;
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
 * ag_service_get_display_name:
 * @service: the #AgService.
 *
 * Returns: the display name of @service.
 */
const gchar *
ag_service_get_display_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    return service->display_name;
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
        if (service->default_settings)
            g_hash_table_unref (service->default_settings);
        g_slice_free (AgService, service);
    }
}

