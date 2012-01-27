/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:ag-service-type
 * @short_description: A description of a service type.
 * @include: libaccounts-glib/ag-service-type.h
 *
 * The #AgServiceType structure represents a service type. The structure is
 * not directly exposed to applications, but its fields are accessible via
 * getter methods.
 * The structure is reference counted. One must use ag_service_type_unref()
 * when done with it.
 */

#include "config.h"
#include "ag-service-type.h"

#include "ag-internals.h"
#include "ag-util.h"
#include <libxml/xmlreader.h>
#include <string.h>

struct _AgServiceType {
    /*< private >*/
    gint ref_count;
    gchar *name;
    gchar *i18n_domain;
    gchar *display_name;
    gchar *icon_name;
    gchar *file_data;
    gsize file_data_len;
};

G_DEFINE_BOXED_TYPE (AgServiceType, ag_service_type,
                     (GBoxedCopyFunc)ag_service_type_ref,
                     (GBoxedFreeFunc)ag_service_type_unref);

static gboolean
parse_service_type (xmlTextReaderPtr reader, AgServiceType *service_type)
{
    const gchar *name;
    int ret, type;

    if (!service_type->name)
    {
        xmlChar *_name = xmlTextReaderGetAttribute (reader,
                                                    (xmlChar *) "id");
        service_type->name = g_strdup ((const gchar *)_name);
        if (_name) xmlFree(_name);
    }

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "service-type") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "name") == 0 && !service_type->display_name)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &service_type->display_name);
            }
            else if (strcmp (name, "icon") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &service_type->icon_name);
            }
            else if (strcmp (name, "translations") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &service_type->i18n_domain);
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
read_service_type_file (xmlTextReaderPtr reader, AgServiceType *service_type)
{
    const xmlChar *name;
    int ret;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = xmlTextReaderConstName (reader);
        if (G_LIKELY (name &&
                      strcmp ((const gchar *)name, "service-type") == 0))
        {
            return parse_service_type (reader, service_type);
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

static AgServiceType *
_ag_service_type_new (void)
{
    AgServiceType *service_type;

    service_type = g_slice_new0 (AgServiceType);
    service_type->ref_count = 1;

    return service_type;
}

static gboolean
_ag_service_type_load_from_file (AgServiceType *service_type)
{
    xmlTextReaderPtr reader;
    gchar *filepath;
    gboolean ret;
    GError *error = NULL;

    g_return_val_if_fail (service_type->name != NULL, FALSE);

    DEBUG_REFS ("Loading service_type %s", service_type->name);
    filepath = _ag_find_libaccounts_file (service_type->name,
                                          ".service-type",
                                          "AG_SERVICE_TYPES",
                                          "accounts/service-types");
    if (G_UNLIKELY (!filepath)) return FALSE;

    g_file_get_contents (filepath, &service_type->file_data,
                         &service_type->file_data_len, &error);
    if (G_UNLIKELY (error))
    {
        g_warning ("Error reading %s: %s", filepath, error->message);
        g_error_free (error);
        g_free (filepath);
        return FALSE;
    }

    /* TODO: cache the xmlReader */
    reader = xmlReaderForMemory (service_type->file_data,
                                 service_type->file_data_len,
                                 filepath, NULL, 0);
    g_free (filepath);
    if (G_UNLIKELY (reader == NULL))
        return FALSE;

    ret = read_service_type_file (reader, service_type);

    xmlFreeTextReader (reader);
    return ret;
}

AgServiceType *
_ag_service_type_new_from_file (const gchar *service_type_name)
{
    AgServiceType *service_type;

    service_type = _ag_service_type_new ();
    service_type->name = g_strdup (service_type_name);
    if (!_ag_service_type_load_from_file (service_type))
    {
        ag_service_type_unref (service_type);
        service_type = NULL;
    }

    return service_type;
}

/**
 * ag_service_type_get_name:
 * @service_type: the #AgServiceType.
 *
 * Get the name of the #AgServiceType.
 *
 * Returns: the name of @service_type.
 */
const gchar *
ag_service_type_get_name (AgServiceType *service_type)
{
    g_return_val_if_fail (service_type != NULL, NULL);
    return service_type->name;
}

/**
 * ag_service_type_get_i18n_domain:
 * @service_type: the #AgServiceType.
 *
 * Get the translation domain of the #AgServiceType.
 *
 * Returns: the translation domain.
 */
const gchar *
ag_service_type_get_i18n_domain (AgServiceType *service_type)
{
    g_return_val_if_fail (service_type != NULL, NULL);
    return service_type->i18n_domain;
}

/**
 * ag_service_type_get_display_name:
 * @service_type: the #AgServiceType.
 *
 * Get the display name of the #AgServiceType.
 *
 * Returns: the display name of @service_type.
 */
const gchar *
ag_service_type_get_display_name (AgServiceType *service_type)
{
    g_return_val_if_fail (service_type != NULL, NULL);
    return service_type->display_name;
}

/**
 * ag_service_type_get_icon_name:
 * @service_type: the #AgServiceType.
 *
 * Get the icon name of the #AgServiceType.
 *
 * Returns: the name of the icon of @service_type.
 */
const gchar *
ag_service_type_get_icon_name (AgServiceType *service_type)
{
    g_return_val_if_fail (service_type != NULL, NULL);
    return service_type->icon_name;
}

/**
 * ag_service_type_get_file_contents:
 * @service_type: the #AgServiceType.
 * @contents: location to receive the pointer to the file contents.
 * @len: location to receive the length of the file, in bytes.
 *
 * Gets the contents of the XML service type file.  The buffer returned in
 * @contents should not be modified or freed, and is guaranteed to be valid as
 * long as @service_type is referenced.
 * If some error occurs, @contents is set to %NULL.
 */
void
ag_service_type_get_file_contents (AgServiceType *service_type,
                                   const gchar **contents,
                                   gsize *len)
{
    g_return_if_fail (service_type != NULL);
    g_return_if_fail (contents != NULL);

    *contents = service_type->file_data;
    if (len)
        *len = service_type->file_data_len;
}

/**
 * ag_service_type_ref:
 * @service_type: the #AgServiceType.
 *
 * Adds a reference to @service_type.
 *
 * Returns: @service_type.
 */
AgServiceType *
ag_service_type_ref (AgServiceType *service_type)
{
    g_return_val_if_fail (service_type != NULL, NULL);
    g_return_val_if_fail (service_type->ref_count > 0, NULL);

    DEBUG_REFS ("Referencing service_type %s (%d)",
                service_type->name, service_type->ref_count);
    service_type->ref_count++;
    return service_type;
}

/**
 * ag_service_type_unref:
 * @service_type: the #AgServiceType.
 *
 * Used to unreference the #AgServiceType structure.
 */
void
ag_service_type_unref (AgServiceType *service_type)
{
    g_return_if_fail (service_type != NULL);
    g_return_if_fail (service_type->ref_count > 0);

    DEBUG_REFS ("Unreferencing service_type %s (%d)",
                service_type->name, service_type->ref_count);
    service_type->ref_count--;
    if (service_type->ref_count == 0)
    {
        g_free (service_type->name);
        g_free (service_type->i18n_domain);
        g_free (service_type->display_name);
        g_free (service_type->icon_name);
        g_free (service_type->file_data);
        g_slice_free (AgServiceType, service_type);
    }
}

