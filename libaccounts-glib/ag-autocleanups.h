/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright 2018 elementary, Inc. (https://elementary.io)
 *
 * Contact: Corentin Noël <corentin@elementary.io>
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

#ifndef _AG_AUTOCLEANUPS_H_
#define _AG_AUTOCLEANUPS_H_

#if !defined (__ACCOUNTS_GLIB_H_INSIDE__) && !defined (ACCOUNTS_GLIB_COMPILATION)
#warning "Only <libaccounts-glib.h> should be included directly."
#endif

#ifndef __GI_SCANNER__
#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgAccount, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgAccountSettingIter, ag_account_settings_iter_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgAccountService, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgApplication, ag_application_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgAuthData, ag_auth_data_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgProvider, ag_provider_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgService, ag_service_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AgServiceType, ag_service_type_unref)

#endif
#endif

#endif /* _AG_AUTOCLEANUPS_H_ */
