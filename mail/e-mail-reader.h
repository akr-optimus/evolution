/*
 * e-mail-reader.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_READER_H
#define E_MAIL_READER_H

#include <gtk/gtk.h>
#include <camel/camel-folder.h>
#include <mail/em-folder-tree-model.h>
#include <mail/em-format-html-display.h>
#include <mail/message-list.h>
#include <shell/e-shell-settings.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_READER \
	(e_mail_reader_get_type ())
#define E_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_READER, EMailReader))
#define E_IS_MAIL_READER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_READER))
#define E_MAIL_READER_GET_IFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_READER, EMailReaderIface))

G_BEGIN_DECLS

typedef struct _EMailReader EMailReader;
typedef struct _EMailReaderIface EMailReaderIface;

struct _EMailReaderIface {
	GTypeInterface parent_iface;

	/* XXX This is getting kinda bloated.  Try to reduce. */
	GtkActionGroup *
			(*get_action_group)	(EMailReader *reader);
	EMFormatHTMLDisplay *
			(*get_display)		(EMailReader *reader);
	CamelFolder *	(*get_folder)		(EMailReader *reader);
	const gchar *	(*get_folder_uri)	(EMailReader *reader);
	gboolean	(*get_hide_deleted)	(EMailReader *reader);
	MessageList *	(*get_message_list)	(EMailReader *reader);
	EShellSettings *(*get_shell_settings)	(EMailReader *reader);
	EMFolderTreeModel *
			(*get_tree_model)	(EMailReader *reader);
	GtkWindow *	(*get_window)		(EMailReader *reader);
};

GType		e_mail_reader_get_type		(void);
void		e_mail_reader_init		(EMailReader *reader);
GtkActionGroup *
		e_mail_reader_get_action_group	(EMailReader *reader);
EMFormatHTMLDisplay *
		e_mail_reader_get_display	(EMailReader *reader);
CamelFolder *	e_mail_reader_get_folder	(EMailReader *reader);
const gchar *	e_mail_reader_get_folder_uri	(EMailReader *reader);
gboolean	e_mail_reader_get_hide_deleted	(EMailReader *reader);
MessageList *	e_mail_reader_get_message_list	(EMailReader *reader);
EShellSettings *e_mail_reader_get_shell_settings(EMailReader *reader);
EMFolderTreeModel *
		e_mail_reader_get_tree_model	(EMailReader *reader);
GtkWindow *	e_mail_reader_get_window	(EMailReader *reader);

G_END_DECLS

#endif /* E_MAIL_READER_H */
