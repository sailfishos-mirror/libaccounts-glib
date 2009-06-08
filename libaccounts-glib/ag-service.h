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

#ifndef _AG_SERVICE_H_
#define _AG_SERVICE_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _AgService AgService;

const gchar *ag_service_get_name (AgService *service);
const gchar *ag_service_get_display_name (AgService *service);
const gchar *ag_service_get_service_type (AgService *service);
const gchar *ag_service_get_provider (AgService *service);
AgService *ag_service_ref (AgService *service);
void ag_service_unref (AgService *service);

G_END_DECLS

#endif /* _AG_SERVICE_H_ */
