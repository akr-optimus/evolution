/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_SESSION_H
#define MAIL_SESSION_H

#include <glib.h>
#include <bonobo/bonobo-ui-component.h>
#include <camel/camel-session.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void mail_session_init (void);
void mail_session_enable_interaction (gboolean enable);
char *mail_session_request_dialog (const char *prompt, gboolean secret,
				   const char *key, gboolean async);
gboolean mail_session_accept_dialog (const char *prompt, const char *key,
				     gboolean async);
char *mail_session_get_password (const char *url);
void mail_session_add_password (const char *url, const char *passwd);
void mail_session_forget_passwords (BonoboUIComponent *uih, void *user_data,
				    const char *path);
void mail_session_remember_password (const char *url);

void mail_session_forget_password (const char *key);

extern CamelSession *session;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_SESSION_H */
