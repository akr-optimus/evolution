/*
 * e-mail-config-format-html.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-config-format-html.h"

#include <shell/e-shell.h>
#include <e-util/e-util.h>
#include <em-format/e-mail-formatter.h>
#include <mail/e-mail-reader-utils.h>

#define E_MAIL_CONFIG_FORMAT_HTML_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_FORMAT_HTML, EMailConfigFormatHTMLPrivate))

struct _EMailConfigFormatHTMLPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigFormatHTML,
	e_mail_config_format_html,
	E_TYPE_EXTENSION)

static void
headers_changed_cb (GSettings *settings,
                    const gchar *key,
                    gpointer user_data)
{
	gint ii;
	gchar **headers;
	EExtension *extension;
	EMailFormatter *formatter;

	g_return_if_fail (settings != NULL);

	if (key && !g_str_equal (key, "headers"))
		return;

	extension = user_data;
	formatter = E_MAIL_FORMATTER (e_extension_get_extensible (extension));

	headers = g_settings_get_strv (settings, "headers");

	e_mail_formatter_clear_headers (formatter);
	for (ii = 0; headers && headers[ii]; ii++) {
		EMailReaderHeader *h;
		const gchar *xml = headers[ii];

		h = e_mail_reader_header_from_xml (xml);
		if (h && h->enabled)
			e_mail_formatter_add_header (
				formatter, h->name, NULL,
				E_MAIL_FORMATTER_HEADER_FLAG_BOLD);

		e_mail_reader_header_free (h);
	}

	if (!headers || !headers[0])
		e_mail_formatter_set_default_headers (formatter);

	g_strfreev (headers);
}

static void
mail_config_format_html_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EShellSettings *shell_settings;
	EShell *shell;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_bind_property_full (
		shell_settings, "mail-citation-color",
		extensible, "citation-color",
		G_BINDING_SYNC_CREATE,
		e_binding_transform_string_to_color,
		NULL, NULL, (GDestroyNotify) NULL);

	g_object_bind_property (
		shell_settings, "mail-mark-citations",
		extensible, "mark-citations",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-image-loading-policy",
		extensible, "image-loading-policy",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-only-local-photos",
		extensible, "only-local-photos",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-sender-photo",
		extensible, "show-sender-photo",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-real-date",
		extensible, "show-real-date",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-animated-images",
		extensible, "animate-images",
		G_BINDING_SYNC_CREATE);

	settings = g_settings_new ("org.gnome.evolution.mail");
	g_signal_connect (settings, "changed", G_CALLBACK (headers_changed_cb), object);

	g_object_set_data_full (
		G_OBJECT (extensible), "reader-header-settings",
		settings, g_object_unref);

	/* Initial synchronization */
	headers_changed_cb (settings, NULL, object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_format_html_parent_class)->
		constructed (object);
}

static void
e_mail_config_format_html_class_init (EMailConfigFormatHTMLClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigFormatHTMLPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_format_html_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER;
}

static void
e_mail_config_format_html_class_finalize (EMailConfigFormatHTMLClass *class)
{
}

static void
e_mail_config_format_html_init (EMailConfigFormatHTML *extension)
{
	extension->priv = E_MAIL_CONFIG_FORMAT_HTML_GET_PRIVATE (extension);
}

void
e_mail_config_format_html_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_format_html_register_type (type_module);
}

