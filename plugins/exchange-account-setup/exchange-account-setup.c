/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sushma Rai <rsushma@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <gconf/gconf-client.h>
#include <camel/camel-provider.h>
#include <camel/camel-url.h>
#include <camel/camel-service.h>
#include <libedataserver/e-xml-hash-utils.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"
#include "widgets/misc/e-error.h"

GtkWidget* org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data);
GtkWidget *org_gnome_exchange_auth_section (EPlugin *epl, EConfigHookItemFactoryData *data);
void org_gnome_exchange_commit (EPlugin *epl, EConfigHookItemFactoryData *data);

/* NB: This should be given a better name, it is NOT a camel service, it is only a camel-exchange one */
typedef gboolean (CamelProviderValidateUserFunc) (CamelURL *camel_url, const char *url, gboolean *remember_password, CamelException *ex);

#define OOF_INFO_FILE_NAME "oof_info.xml"

typedef struct {
        CamelProviderValidateUserFunc *validate_user;
}CamelProviderValidate;

CamelServiceAuthType camel_exchange_ntlm_authtype = {
        /* i18n: "Secure Password Authentication" is an Outlookism */
        N_("Secure Password"),

        /* i18n: "NTLM" probably doesn't translate */
        N_("This option will connect to the Exchange server using "
           "secure password (NTLM) authentication."),

        "",
        TRUE
};

CamelServiceAuthType camel_exchange_password_authtype = {
        N_("Plaintext Password"),

        N_("This option will connect to the Exchange server using "
           "standard plaintext password authentication."),

        "Basic",
        TRUE
};


typedef struct {
	gboolean state;
	char *account_name, *message;
	GtkWidget *text_view;
}OOFData;

OOFData *oof_data;

static void
update_state (GtkTextBuffer *buffer, gpointer data)
{
	if (gtk_text_buffer_get_modified (buffer)) {
		GtkTextIter start, end;
		if (oof_data->message)
			g_free (oof_data->message);
		gtk_text_buffer_get_bounds (buffer, &start, &end);
		oof_data->message =  gtk_text_buffer_get_text (buffer, &start,
							       &end, FALSE);
		gtk_text_buffer_set_modified (buffer, FALSE);
	}
}

static void 
toggled_state (GtkToggleButton *button, gpointer data)
{
	gboolean current_oof_state = gtk_toggle_button_get_active (button);

	if (current_oof_state == oof_data->state)
		return; 
	oof_data->state = current_oof_state;
	gtk_widget_set_sensitive (oof_data->text_view, current_oof_state);
}


/* only used in editor */
GtkWidget *
org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const char *source_url;
	CamelURL *url;
	GtkWidget *oof_page;
	GtkWidget *oof_table;
	GtkWidget *oof_description, *label_status, *label_empty;
	GtkWidget *radiobutton_inoff, *radiobutton_oof;
	GtkWidget *vbox_oof, *vbox_oof_message;
	GtkWidget *oof_frame;
	GtkWidget *scrolledwindow_oof;
	GtkWidget *textview_oof;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *txt, *oof_message, *oof_info_file, *base_dir;
	GHashTable *oof_props;
	xmlDoc *doc;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);
		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	oof_data = g_new0 (OOFData, 1);

	/* See if oof info found already */
	
	oof_data->account_name =  g_strdup_printf ("%s@%s", url->user, url->host);
	base_dir = g_strdup_printf ("%s/.evolution/exchange/%s", 
				    g_get_home_dir (), oof_data->account_name);

	oof_info_file = g_build_filename (base_dir, OOF_INFO_FILE_NAME, NULL);
	g_free(base_dir);

	oof_data->state = FALSE;
	oof_data->message = NULL;
	oof_data->text_view = NULL;

	if (g_file_test (oof_info_file, G_FILE_TEST_EXISTS)) {
		doc = xmlParseFile (oof_info_file);
		if (doc) {
			char *status, *message;

			oof_props = e_xml_to_hash (doc, E_XML_HASH_TYPE_PROPERTY);
			xmlFreeDoc (doc);
			status = g_hash_table_lookup (oof_props, "oof-state");
			if (!strcmp (status, "oof")) {
				oof_data->state = TRUE;
				message = g_hash_table_lookup (oof_props, "oof-message");
				if (message && *message)
					oof_data->message = g_strdup (message);
				else
					oof_data->message = NULL;
			}
			g_hash_table_destroy (oof_props);
		}
	}
	g_free (oof_info_file);
	
	/* construct page */

	oof_page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (oof_page), 12);

	/* Description section */

	oof_description = gtk_label_new (_("The message specified below will be automatically sent to \neach person who sends mail to you while you are out of the office."));
	gtk_label_set_justify (GTK_LABEL (oof_description), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (oof_description), TRUE);
	gtk_misc_set_alignment (GTK_MISC (oof_description), 0.5, 0.5);
	gtk_misc_set_padding (GTK_MISC (oof_description), 0, 18);
	
	gtk_box_pack_start (GTK_BOX (oof_page), oof_description, FALSE, TRUE, 0);

	/* Table with out of office radio buttons */

	oof_table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (oof_table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (oof_table), 6);
	gtk_box_pack_start (GTK_BOX (oof_page), oof_table, FALSE, FALSE, 0);

	/* translators: exchange out of office status header */
	txt = g_strdup_printf("<b>%s</b>", _("Status:"));
	label_status = gtk_label_new (txt);
	g_free(txt);
	gtk_label_set_justify (GTK_LABEL (label_status), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label_status), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_status), 0, 0); 
	gtk_label_set_use_markup (GTK_LABEL (label_status), TRUE);
	gtk_table_attach (GTK_TABLE (oof_table), label_status, 0, 1, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0); 

	if (oof_data->state) {
		radiobutton_oof = gtk_radio_button_new_with_label (NULL,
					_("I am out of the office"));
		radiobutton_inoff = gtk_radio_button_new_with_label_from_widget (
					GTK_RADIO_BUTTON (radiobutton_oof),
					_("I am in the office"));
	}
	else {
		radiobutton_inoff = gtk_radio_button_new_with_label (NULL,
						_("I am in the office"));
		radiobutton_oof = gtk_radio_button_new_with_label_from_widget (
					GTK_RADIO_BUTTON (radiobutton_inoff),
					_("I am out of the office"));
	}

	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_inoff, 1, 2, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0);

	label_empty = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (label_empty), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label_empty), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_empty), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (label_empty), FALSE);
	gtk_table_attach (GTK_TABLE (oof_table), label_empty, 0, 1, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_oof, 1, 2, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);

	g_signal_connect (radiobutton_oof, "toggled", G_CALLBACK (toggled_state), NULL);

	/* frame containg oof message text box */

	vbox_oof = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (oof_page), vbox_oof, FALSE, FALSE, 0);

	oof_frame = gtk_frame_new ("");
	gtk_container_set_border_width (GTK_CONTAINER (oof_frame), 1); 
	gtk_frame_set_shadow_type (GTK_FRAME (oof_frame), GTK_SHADOW_ETCHED_IN);
	gtk_frame_set_label (GTK_FRAME (oof_frame), _("Out of office Message:"));
	gtk_box_pack_start (GTK_BOX (vbox_oof), oof_frame, FALSE, FALSE, 0);

	vbox_oof_message = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (oof_frame), vbox_oof_message);
	
	scrolledwindow_oof = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_SHADOW_IN); 
	gtk_box_pack_start (GTK_BOX (vbox_oof_message), 
			    scrolledwindow_oof, TRUE, TRUE, 0);

	textview_oof = gtk_text_view_new(); 
	gtk_text_view_set_justification (GTK_TEXT_VIEW (textview_oof), 
					 GTK_JUSTIFY_LEFT);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview_oof), 
				     GTK_WRAP_WORD);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview_oof), TRUE);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview_oof));
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	oof_message = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	if (oof_message && *oof_message) {
		/* Will this ever happen? */
		oof_data->message = oof_message;
	}
	if (oof_data->message) {
		/* previuosly set message */
		gtk_text_buffer_set_text (buffer, oof_data->message, -1);
		gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview_oof), buffer);
		
	}
	gtk_text_buffer_set_modified (buffer, FALSE);
	if (!oof_data->state)
		gtk_widget_set_sensitive (textview_oof, FALSE);
	oof_data->text_view = textview_oof;
	g_signal_connect (buffer, "changed", G_CALLBACK (update_state), NULL);

	gtk_container_add (GTK_CONTAINER (scrolledwindow_oof), textview_oof);	
	gtk_widget_show_all (scrolledwindow_oof);

	gtk_widget_show_all (oof_page);
	gtk_notebook_insert_page (GTK_NOTEBOOK (data->parent), oof_page, gtk_label_new(_("Exchange Settings")), 4);
	return oof_page;
}

static void
owa_authenticate_user(GtkWidget *button, EConfig *config)
{
	EMConfigTargetAccount *target_account = (EMConfigTargetAccount *)config->target;
	CamelProviderValidate *validate;
	CamelURL *url=NULL;
	CamelProvider *provider = NULL;
	gboolean remember_password;
	char *url_string; 
	const char *source_url, *id_name;
	char *at, *user;

	source_url = e_account_get_string (target_account->account, E_ACCOUNT_SOURCE_URL);
	provider = camel_provider_get (source_url, NULL);
	if (!provider || provider->priv == NULL) {
		/* can't happen? */
		return;
	}

	url = camel_url_new(source_url, NULL);
	validate = provider->priv; 
	if (url->user == NULL) {
		id_name = e_account_get_string (target_account->account, E_ACCOUNT_ID_ADDRESS);
		if (id_name) {
			at = strchr(id_name, '@');
			user = g_alloca(at-id_name+1);
			memcpy(user, id_name, at-id_name);
			user[at-id_name] = 0; 
			camel_url_set_user (url, user);
		}
	}

	/* validate_user() CALLS GTK!!!

	   THIS IS TOTALLY UNNACCEPTABLE!!!!!!!!

	   It must use camel_session_ask_password, and it should return an exception for any problem,
	   which should then be shown using e-error */

	if (validate->validate_user(url, camel_url_get_param(url, "owa_url"), &remember_password, NULL)) {
		url_string = camel_url_to_string (url, 0);
		e_account_set_string(target_account->account, E_ACCOUNT_SOURCE_URL, url_string);
		e_account_set_string(target_account->account, E_ACCOUNT_TRANSPORT_URL, url_string);
		e_account_set_bool(target_account->account, E_ACCOUNT_SOURCE_SAVE_PASSWD, remember_password);
		g_free(url_string);
	}
	
	camel_url_free (url);
}

static void
owa_editor_entry_changed(GtkWidget *entry, EConfig *config)
{
	const char *uri, *ssl = NULL;
	CamelURL *url, *owaurl = NULL;
	char *url_string;
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;
	GtkWidget *button = g_object_get_data((GObject *)entry, "authenticate-button");
	int active = FALSE;

	/* NB: we set the button active only if we have a parsable uri entered */

	url = camel_url_new(e_account_get_string(target->account, E_ACCOUNT_SOURCE_URL), NULL);
	uri = gtk_entry_get_text((GtkEntry *)entry);
	if (uri && uri[0]) {
		camel_url_set_param(url, "owa_url", uri);
		owaurl = camel_url_new(uri, NULL);
		if (owaurl) {
			active = TRUE;

			/* Reading the owa url and setting use_ssl paramemter */
			if (!strcmp(owaurl->protocol, "https"))
				ssl = "always";
			camel_url_free(owaurl);
		}
	} else {
		camel_url_set_param(url, "owa_url", NULL);
	}

	camel_url_set_param(url, "use_ssl", ssl);
	gtk_widget_set_sensitive(button, active);

	url_string = camel_url_to_string(url, 0);
	e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, url_string);
	g_free(url_string);
}

static void
destroy_label(GtkWidget *old, GtkWidget *label)
{
	gtk_widget_destroy(label);
}

static char *
construct_owa_url (CamelURL *url) 
{
	const char *owa_path, *use_ssl = NULL;
	const char *protocol = "http", *mailbox_name;
	char *owa_url;

	use_ssl = camel_url_get_param (url, "use_ssl");
	if (use_ssl) {
		if (!strcmp (use_ssl, "always"))
			protocol = "https";
	}

	owa_path = camel_url_get_param (url, "owa_path");
	if (!owa_path)
		owa_path = "/exchange";
	mailbox_name = camel_url_get_param (url, "mailbox");

	if (mailbox_name)
		owa_url = g_strdup_printf("%s://%s%s/%s", protocol, url->host, owa_path, mailbox_name); 
	else
		owa_url = g_strdup_printf("%s://%s%s", protocol, url->host, owa_path ); 

	return owa_url;
}

/* used by editor and druid - same code */
GtkWidget *
org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const char *source_url;
	char *owa_url = NULL;
	GtkWidget *owa_entry;
	CamelURL *url;
	int row;
	GtkWidget *hbox, *label, *button;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);

		if (data->old
		    && (label = g_object_get_data((GObject *)data->old, "authenticate-label")))
			gtk_widget_destroy(label);

		/* TODO: we could remove 'owa-url' from the url,
		   but that will lose it if we come back.  Maybe a commit callback could do it */

		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	owa_url = g_strdup (camel_url_get_param(url, "owa_url"));

	/* if the host is null, then user+other info is dropped silently, force it to be kept */
	if (url->host == NULL) {
		char *uri;

		camel_url_set_host(url, "");
		uri = camel_url_to_string(url, 0);
		e_account_set_string(target_account->account,  E_ACCOUNT_SOURCE_URL, uri);
		g_free(uri);
	}

	row = ((GtkTable *)data->parent)->nrows;

	hbox = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_OWA Url:"));
	gtk_widget_show(label);

	owa_entry = gtk_entry_new();

	if (!owa_url) {
		if (url->host[0] != 0) {
			char *uri;

			/* url has hostname but not owa_url. 
			 * Account has been created using x-c-s or evo is upgraded to 2.2
		 	 * When invoked from druid, hostname will get set after validation,
		 	 * so this condition will never be true during account creation.
			 */
			owa_url = construct_owa_url (url);
			camel_url_set_param (url, "owa_url", owa_url);
			uri = camel_url_to_string(url, 0);
			e_account_set_string(target_account->account,  E_ACCOUNT_SOURCE_URL, uri);
			g_free(uri);
		}
	}
	if (owa_url)
		gtk_entry_set_text(GTK_ENTRY (owa_entry), owa_url); 
	gtk_label_set_mnemonic_widget((GtkLabel *)label, owa_entry);

	button = gtk_button_new_with_mnemonic (_("A_uthenticate"));
	gtk_widget_set_sensitive (button, owa_url && owa_url[0]);

	gtk_box_pack_start (GTK_BOX (hbox), owa_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	gtk_table_attach (GTK_TABLE (data->parent), label, 0, 1, row, row+1, 0, 0, 0, 0); 
	gtk_table_attach (GTK_TABLE (data->parent), hbox, 1, 2, row, row+1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0); 

	g_signal_connect (owa_entry, "changed", G_CALLBACK(owa_editor_entry_changed), data->config);
	g_object_set_data((GObject *)owa_entry, "authenticate-button", button);
	g_signal_connect (button, "clicked", G_CALLBACK(owa_authenticate_user), data->config);

	/* Track the authenticate label, so we can destroy it if e-config is to destroy the hbox */
	g_object_set_data((GObject *)hbox, "authenticate-label", label);

	g_free (owa_url);
	return hbox;
}

gboolean
org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)data->config->target;
	int status = TRUE;

	/* We assume that if the host is set, then the setting is valid.
	   The host gets set when the provider validate() call is made */
	/* We do this check for receive page also, so that user can
	 * proceed with the account set up only after user is validated,
	 * and host name is reset by validate() call
	 */
	if (data->pageid == NULL ||
	    strcmp (data->pageid, "10.receive") == 0 ||
	    strcmp (data->pageid, "20.receive_options") == 0) {
		CamelURL *url;

		url = camel_url_new(e_account_get_string(target->account,  E_ACCOUNT_SOURCE_URL), NULL);
		/* Note: we only care about exchange url's, we WILL get called on all other url's too. */
		if (url != NULL
		    && strcmp(url->protocol, "exchange") == 0
		    && (url->host == NULL || url->host[0] == 0))
			status = FALSE;

		if (url)
			camel_url_free(url);
	}

	return status;
}

static void 
store_oof_info ()
{
	char *oof_storage_base_dir, *oof_storage_file, *status;
	xmlDocPtr doc;
	int result;
	GHashTable *oof_props;

	/* oof information is entered at editor need to be written to the 
	 * server, dump it to the file */
	oof_storage_base_dir = g_strdup_printf ("%s/.evolution/exchange/%s",
				    g_get_home_dir (), oof_data->account_name);

	if (!g_file_test (oof_storage_base_dir, G_FILE_TEST_EXISTS)) {
		if (mkdir (oof_storage_base_dir, 0755)) {
			/* Failed to create file */
			g_free (oof_storage_base_dir);
			return;
		}
	}

	oof_storage_file = g_build_filename (oof_storage_base_dir, 
					     OOF_INFO_FILE_NAME, NULL);
	if (g_file_test (oof_storage_file, G_FILE_TEST_EXISTS))
		unlink (oof_storage_file);
		
	if (oof_data->state)
		status = g_strdup ("oof");
	else
		status = g_strdup ("in-office");

	oof_props = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (oof_props, "oof-state", status);
	g_hash_table_insert (oof_props, "sync-state", g_strdup("0"));
	g_hash_table_insert (oof_props, "oof-message", oof_data->message);
	doc = e_xml_from_hash (oof_props, E_XML_HASH_TYPE_PROPERTY, "oof-info");
	result = xmlSaveFile (oof_storage_file, doc);
	xmlFreeDoc (doc);
	if (result < 0)
		unlink (oof_storage_file);
	g_hash_table_destroy (oof_props);
	g_free (status);
	g_free (oof_storage_file);
	g_free (oof_storage_base_dir);
}

static void
destroy_oof_data ()
{
	if (oof_data->account_name)
		g_free (oof_data->account_name);
	if (oof_data->message)
		g_free (oof_data->message);
	g_free (oof_data);	
}

void
org_gnome_exchange_commit (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	target_account = (EMConfigTargetAccount *)data->config->target;
	const char *source_url;
	CamelURL *url;
	
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new (source_url, NULL);
	if (url == NULL
	    || strcmp (url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free (url);

		return;
	}
	if (data->old) {
		camel_url_free(url);
		return;
	}

	/* dump oof data so that can be set in exchange account */
	store_oof_info ();
	destroy_oof_data ();
	return;
}

static void
exchange_check_authtype (GtkWidget *w, EConfig *config)
{
	return;
}

static void 
exchange_authtype_changed (GtkComboBox *dropdown, EConfig *config)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;
	int id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;
	CamelURL *url_source, *url_transport;
	const char *source_url, *transport_url;
	char *source_url_string, *transport_url_string;

	source_url = e_account_get_string (target->account,
					   E_ACCOUNT_SOURCE_URL);
	if (id == -1)
		return;

	url_source = camel_url_new (source_url, NULL);

	transport_url = e_account_get_string (target->account,
					      E_ACCOUNT_TRANSPORT_URL);
	url_transport = camel_url_new (transport_url, NULL);
	
	model = gtk_combo_box_get_model(dropdown);
	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, id)) {
		gtk_tree_model_get(model, &iter, 1, &authtype, -1);
		if (authtype) {
			camel_url_set_authmech(url_source, authtype->authproto);
			camel_url_set_authmech(url_transport, authtype->authproto);
		}
		else {
			camel_url_set_authmech(url_source, NULL);
			camel_url_set_authmech(url_transport, NULL);
		}
	
		source_url_string = camel_url_to_string(url_source, 0);
		transport_url_string = camel_url_to_string(url_transport, 0);
		e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, source_url_string);
		e_account_set_string(target->account, E_ACCOUNT_TRANSPORT_URL, transport_url_string);
		g_free(source_url_string);
		g_free(transport_url_string);
	}
	camel_url_free(url_source);
	camel_url_free(url_transport);
}


GtkWidget *
org_gnome_exchange_auth_section (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const char *source_url; 
	char *label_text;
	CamelURL *url;
	GtkWidget *hbox, *button, *auth_label, *vbox, *label_hide;
	GtkComboBox *dropdown;
	GtkTreeIter iter;
	GtkListStore *store;
	int i, active=0, auth_changed_id = 0;
	GList *authtypes, *l, *ll;
	
	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account, 
					   E_ACCOUNT_SOURCE_URL);
	url = camel_url_new (source_url, NULL);
	if (url == NULL
	    || strcmp (url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free (url);

		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	vbox = gtk_vbox_new (FALSE, 6);

	label_text = g_strdup_printf("<b>%s</b>", _("Authentication Type"));
	auth_label = gtk_label_new (label_text);
	g_free (label_text);
	gtk_label_set_justify (GTK_LABEL (auth_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (auth_label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (auth_label), 0, 0); 
	gtk_label_set_use_markup (GTK_LABEL (auth_label), TRUE);

	label_hide = gtk_label_new("\n");

	hbox = gtk_hbox_new (FALSE, 6);

	dropdown = (GtkComboBox * )gtk_combo_box_new ();

	button = gtk_button_new_with_mnemonic (_("Ch_eck for Supported Types"));

	authtypes = g_list_prepend (g_list_prepend (NULL, &camel_exchange_password_authtype),
				    &camel_exchange_ntlm_authtype);
	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	for (i=0, l=authtypes; l; l=l->next, i++) {
		CamelServiceAuthType *authtype = l->data;
		int avail = TRUE;

		if (authtypes) {
			for (ll = authtypes; ll; ll = g_list_next(ll))
				if (!strcmp(authtype->authproto, 
					((CamelServiceAuthType *)ll->data)->authproto))
					break;
			avail = ll != NULL;
		}
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, authtype->name, 1, 
				    authtype, 2, !avail, -1);

		if (url && url->authmech && !strcmp(url->authmech, authtype->authproto))
			active = i;
	}

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, -1);

	if (auth_changed_id == 0) {
		GtkCellRenderer *cell = gtk_cell_renderer_text_new();

		gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
		gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, 
						"text", 0, "strikethrough", 2, NULL);

		auth_changed_id = g_signal_connect (dropdown, 
						    "changed", 
						    G_CALLBACK (exchange_authtype_changed), 
						    data->config);
		g_signal_connect (button, 
				  "clicked", 
				  G_CALLBACK(exchange_check_authtype), 
				  data->config);
	}

	gtk_combo_box_set_active(dropdown, active);

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (dropdown), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), auth_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_hide, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);

	gtk_box_pack_start (GTK_BOX (data->parent), vbox, TRUE, TRUE, 0);	

	if (url)
		camel_url_free(url);
	g_list_free (authtypes);

	return vbox;
}
