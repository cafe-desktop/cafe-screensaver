/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *          Rodrigo Moya <rodrigo@novell.com>
 *
 */

#include "config.h"

#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* For uid_t, gid_t */

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <ctk/ctk.h>

#include <gio/gio.h>

#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-desktop-utils.h>

#include "gs-debug.h"

#include "copy-theme-dialog.h"

#include "gs-theme-manager.h"
#include "gs-job.h"
#include "gs-prefs.h" /* for GS_MODE enum */

#define LOCKDOWN_SETTINGS_SCHEMA "org.cafe.lockdown"
#define KEY_LOCK_DISABLE "disable-lock-screen"

#define SESSION_SETTINGS_SCHEMA "org.cafe.session"
#define KEY_IDLE_DELAY "idle-delay"

#define GSETTINGS_SCHEMA "org.cafe.screensaver"
#define KEY_LOCK "lock-enabled"
#define KEY_IDLE_ACTIVATION_ENABLED "idle-activation-enabled"
#define KEY_MODE "mode"
#define KEY_LOCK_DELAY "lock-delay"
#define KEY_CYCLE_DELAY "cycle-delay"
#define KEY_THEMES "themes"

#define GPM_COMMAND "cafe-power-preferences"

enum
{
    NAME_COLUMN = 0,
    ID_COLUMN,
    N_COLUMNS
};

/* Drag and drop info */
enum
{
    TARGET_URI_LIST,
    TARGET_NS_URL
};

static CtkTargetEntry drop_types [] =
{
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "_NETSCAPE_URL", 0, TARGET_NS_URL }
};

static CtkBuilder     *builder = NULL;
static GSThemeManager *theme_manager = NULL;
static GSJob          *job = NULL;
static GSettings      *screensaver_settings = NULL;
static GSettings      *session_settings = NULL;
static GSettings      *lockdown_settings = NULL;

static gint32
config_get_activate_delay (gboolean *is_writable)
{
	gint32 delay;

	if (is_writable)
	{
		*is_writable = g_settings_is_writable (session_settings,
		               KEY_IDLE_DELAY);
	}

	delay = g_settings_get_int (session_settings, KEY_IDLE_DELAY);

	if (delay < 1)
	{
		delay = 1;
	}

	return delay;
}

static void
config_set_activate_delay (gint32 timeout)
{
	g_settings_set_int (session_settings, KEY_IDLE_DELAY, timeout);
}

static int
config_get_mode (gboolean *is_writable)
{
	int mode;

	if (is_writable)
	{
		*is_writable = g_settings_is_writable (screensaver_settings,
		               KEY_MODE);
	}

	mode = g_settings_get_enum (screensaver_settings, KEY_MODE);

	return mode;
}

static void
config_set_mode (int mode)
{
	g_settings_set_enum (screensaver_settings, KEY_MODE, mode);
}

static char *
config_get_theme (gboolean *is_writable)
{
	char *name;
	int   mode;

	if (is_writable)
	{
		gboolean can_write_theme;
		gboolean can_write_mode;

		can_write_theme = g_settings_is_writable (screensaver_settings,
		                                          KEY_THEMES);
		can_write_mode = g_settings_is_writable (screensaver_settings,
		                                         KEY_MODE);
		*is_writable = can_write_theme && can_write_mode;
	}

	mode = config_get_mode (NULL);

	name = NULL;
	if (mode == GS_MODE_BLANK_ONLY)
	{
		name = g_strdup ("__blank-only");
	}
	else if (mode == GS_MODE_RANDOM)
	{
		name = g_strdup ("__random");
	}
	else
	{
		gchar **strv;
		strv = g_settings_get_strv (screensaver_settings,
	                                KEY_THEMES);
		if (strv != NULL) {
			name = g_strdup (strv[0]);
		}
		else
		{
			/* TODO: handle error */
			/* default to blank */
			name = g_strdup ("__blank-only");
		}

		g_strfreev (strv);
	}

	return name;
}

static gchar **
get_all_theme_ids (GSThemeManager *theme_manager)
{
	gchar **ids = NULL;
	GSList *entries;
	GSList *l;
        guint idx = 0;

	entries = gs_theme_manager_get_info_list (theme_manager);
        ids = g_new0 (gchar *, g_slist_length (entries) + 1);
	for (l = entries; l; l = l->next)
	{
		GSThemeInfo *info = l->data;

		ids[idx++] = g_strdup (gs_theme_info_get_id (info));
		gs_theme_info_unref (info);
	}
	g_slist_free (entries);

	return ids;
}

static void
config_set_theme (const char *theme_id)
{
	gchar **strv = NULL;
	int     mode;

	if (theme_id && strcmp (theme_id, "__blank-only") == 0)
	{
		mode = GS_MODE_BLANK_ONLY;
	}
	else if (theme_id && strcmp (theme_id, "__random") == 0)
	{
		mode = GS_MODE_RANDOM;

		/* set the themes key to contain all available screensavers */
		strv = get_all_theme_ids (theme_manager);
	}
	else
	{
		mode = GS_MODE_SINGLE;
		strv = g_strsplit (theme_id, "%%%", 1);
	}

	config_set_mode (mode);

	g_settings_set_strv (screensaver_settings,
	                     KEY_THEMES,
	                     (const gchar * const*) strv);

	g_strfreev (strv);

}

static gboolean
config_get_enabled (gboolean *is_writable)
{
	int enabled;

	if (is_writable)
	{
		*is_writable = g_settings_is_writable (screensaver_settings,
		               KEY_LOCK);
	}

	enabled = g_settings_get_boolean (screensaver_settings, KEY_IDLE_ACTIVATION_ENABLED);

	return enabled;
}

static void
config_set_enabled (gboolean enabled)
{
	g_settings_set_boolean (screensaver_settings, KEY_IDLE_ACTIVATION_ENABLED, enabled);
}

static gboolean
config_get_lock (gboolean *is_writable)
{
	gboolean lock;

	if (is_writable)
	{
		*is_writable = g_settings_is_writable (screensaver_settings,
		               KEY_LOCK);
	}

	lock = g_settings_get_boolean (screensaver_settings, KEY_LOCK);

	return lock;
}

static gboolean
config_get_lock_disabled ()
{
	return g_settings_get_boolean (lockdown_settings, KEY_LOCK_DISABLE);
}

static void
config_set_lock (gboolean lock)
{
	g_settings_set_boolean (screensaver_settings, KEY_LOCK, lock);
}

static void
job_set_theme (GSJob      *job,
               const char *theme)
{
	GSThemeInfo *info;
	const char  *command;

	command = NULL;

	info = gs_theme_manager_lookup_theme_info (theme_manager, theme);
	if (info != NULL)
	{
		command = gs_theme_info_get_exec (info);
	}

	gs_job_set_command (job, command);

	if (info != NULL)
	{
		gs_theme_info_unref (info);
	}
}

static gboolean
preview_on_draw (CtkWidget *widget,
                 cairo_t   *cr,
                 gpointer   data)
{
	if (job == NULL || !gs_job_is_running (job))
	{
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_paint (cr);
	}

	return FALSE;
}

static void
preview_set_theme (CtkWidget  *widget,
                   const char *theme,
                   const char *name)
{
	CtkWidget *label;
	char      *markup;

	if (job != NULL)
	{
		gs_job_stop (job);
	}

	ctk_widget_queue_draw (widget);

	label = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_theme_label"));
	markup = g_markup_printf_escaped ("<i>%s</i>", name);
	ctk_label_set_markup (CTK_LABEL (label), markup);
	g_free (markup);

	if ((theme && strcmp (theme, "__blank-only") == 0))
	{

	}
	else if (theme && strcmp (theme, "__random") == 0)
	{
		gchar **themes;

		themes = get_all_theme_ids (theme_manager);
		if (themes != NULL)
		{
			gint32  i;

			i = g_random_int_range (0, g_strv_length (themes));
                        job_set_theme (job, themes[i]);
                        g_strfreev (themes);

			gs_job_start (job);
		}
	}
	else
	{
		job_set_theme (job, theme);
		gs_job_start (job);
	}
}

static void
help_display (void)
{
	GError *error;

	error = NULL;
	ctk_show_uri_on_window (NULL,
                                "help:cafe-user-guide/prefs-screensaver",
                                GDK_CURRENT_TIME,
                                &error);

	if (error != NULL)
	{
		CtkWidget *d;

		d = ctk_message_dialog_new (NULL,
		                            CTK_DIALOG_MODAL | CTK_DIALOG_DESTROY_WITH_PARENT,
		                            CTK_MESSAGE_ERROR, CTK_BUTTONS_OK,
		                            "%s", error->message);
		ctk_dialog_run (CTK_DIALOG (d));
		ctk_widget_destroy (d);
		g_error_free (error);
	}

}

static void
response_cb (CtkWidget *widget,
             int        response_id)
{

	if (response_id == CTK_RESPONSE_HELP)
	{
		help_display ();
	}
	else if (response_id == CTK_RESPONSE_REJECT)
	{
		GError  *error;
		gboolean res;

		error = NULL;

		res = cafe_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
		                                        GPM_COMMAND,
		                                        &error);
		if (! res)
		{
			g_warning ("Unable to start power management preferences: %s", error->message);
			g_error_free (error);
		}
	}
	else
	{
		ctk_widget_destroy (widget);
		ctk_main_quit ();
	}
}

static GSList *
get_theme_info_list (void)
{
	return gs_theme_manager_get_info_list (theme_manager);
}

static void
populate_model (CtkTreeStore *store)
{
	CtkTreeIter iter;
	GSList     *themes        = NULL;
	GSList     *l;

	ctk_tree_store_append (store, &iter, NULL);
	ctk_tree_store_set (store, &iter,
	                    NAME_COLUMN, _("Blank screen"),
	                    ID_COLUMN, "__blank-only",
	                    -1);

	ctk_tree_store_append (store, &iter, NULL);
	ctk_tree_store_set (store, &iter,
	                    NAME_COLUMN, _("Random"),
	                    ID_COLUMN, "__random",
	                    -1);

	ctk_tree_store_append (store, &iter, NULL);
	ctk_tree_store_set (store, &iter,
	                    NAME_COLUMN, NULL,
	                    ID_COLUMN, "__separator",
	                    -1);

	themes = get_theme_info_list ();

	if (themes == NULL)
	{
		return;
	}

	for (l = themes; l; l = l->next)
	{
		const char  *name;
		const char  *id;
		GSThemeInfo *info = l->data;

		if (info == NULL)
		{
			continue;
		}

		name = gs_theme_info_get_name (info);
		id = gs_theme_info_get_id (info);

		ctk_tree_store_append (store, &iter, NULL);
		ctk_tree_store_set (store, &iter,
		                    NAME_COLUMN, name,
		                    ID_COLUMN, id,
		                    -1);

		gs_theme_info_unref (info);
	}

	g_slist_free (themes);
}

static void
tree_selection_previous (CtkTreeSelection *selection)
{
	CtkTreeIter   iter;
	CtkTreeModel *model;
	CtkTreePath  *path;

	if (! ctk_tree_selection_get_selected (selection, &model, &iter))
	{
		return;
	}

	path = ctk_tree_model_get_path (model, &iter);
	if (ctk_tree_path_prev (path))
	{
		ctk_tree_selection_select_path (selection, path);
	}
}

static void
tree_selection_next (CtkTreeSelection *selection)
{
	CtkTreeIter   iter;
	CtkTreeModel *model;
	CtkTreePath  *path;

	if (! ctk_tree_selection_get_selected (selection, &model, &iter))
	{
		return;
	}

	path = ctk_tree_model_get_path (model, &iter);
	ctk_tree_path_next (path);
	ctk_tree_selection_select_path (selection, path);
}

static void
tree_selection_changed_cb (CtkTreeSelection *selection,
                           CtkWidget        *preview)
{
	CtkTreeIter   iter;
	CtkTreeModel *model;
	char         *theme;
	char         *name;

	if (! ctk_tree_selection_get_selected (selection, &model, &iter))
	{
		return;
	}

	ctk_tree_model_get (model, &iter, ID_COLUMN, &theme, NAME_COLUMN, &name, -1);

	if (theme == NULL)
	{
		g_free (name);
		return;
	}

	preview_set_theme (preview, theme, name);
	config_set_theme (theme);

	g_free (theme);
	g_free (name);
}

static void
activate_delay_value_changed_cb (CtkRange *range,
                                 gpointer  user_data)
{
	gdouble value;

	value = ctk_range_get_value (range);
	config_set_activate_delay ((gint32)value);
}

static int
compare_theme_names (char *name_a,
                     char *name_b,
                     char *id_a,
                     char *id_b)
{

	if (id_a == NULL)
	{
		return 1;
	}
	else if (id_b == NULL)
	{
		return -1;
	}

	if (strcmp (id_a, "__blank-only") == 0)
	{
		return -1;
	}
	else if (strcmp (id_b, "__blank-only") == 0)
	{
		return 1;
	}
	else if (strcmp (id_a, "__random") == 0)
	{
		return -1;
	}
	else if (strcmp (id_b, "__random") == 0)
	{
		return 1;
	}
	else if (strcmp (id_a, "__separator") == 0)
	{
		return -1;
	}
	else if (strcmp (id_b, "__separator") == 0)
	{
		return 1;
	}

	if (name_a == NULL)
	{
		return 1;
	}
	else if (name_b == NULL)
	{
		return -1;
	}

	return g_utf8_collate (name_a, name_b);
}

static int
compare_theme  (CtkTreeModel *model,
                CtkTreeIter  *a,
                CtkTreeIter  *b,
                gpointer      user_data)
{
	char *name_a;
	char *name_b;
	char *id_a;
	char *id_b;
	int   result;

	ctk_tree_model_get (model, a, NAME_COLUMN, &name_a, -1);
	ctk_tree_model_get (model, b, NAME_COLUMN, &name_b, -1);
	ctk_tree_model_get (model, a, ID_COLUMN, &id_a, -1);
	ctk_tree_model_get (model, b, ID_COLUMN, &id_b, -1);

	result = compare_theme_names (name_a, name_b, id_a, id_b);

	g_free (name_a);
	g_free (name_b);
	g_free (id_a);
	g_free (id_b);

	return result;
}

static gboolean
separator_func (CtkTreeModel *model,
                CtkTreeIter  *iter,
                gpointer      data)
{
	int   column = GPOINTER_TO_INT (data);
	char *text;

	ctk_tree_model_get (model, iter, column, &text, -1);

	if (text != NULL && strcmp (text, "__separator") == 0)
	{
		return TRUE;
	}

	g_free (text);

	return FALSE;
}

static void
setup_treeview (CtkWidget *tree,
                CtkWidget *preview)
{
	CtkTreeStore      *store;
	CtkTreeViewColumn *column;
	CtkCellRenderer   *renderer;
	CtkTreeSelection  *select;

	store = ctk_tree_store_new (N_COLUMNS,
	                            G_TYPE_STRING,
	                            G_TYPE_STRING);
	populate_model (store);

	ctk_tree_view_set_model (CTK_TREE_VIEW (tree),
	                         CTK_TREE_MODEL (store));

	g_object_unref (store);

	g_object_set (tree, "show-expanders", FALSE, NULL);

	renderer = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes ("Name", renderer,
	         "text", NAME_COLUMN,
	         NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (tree), column);

	ctk_tree_view_column_set_sort_column_id (column, NAME_COLUMN);
	ctk_tree_sortable_set_sort_func (CTK_TREE_SORTABLE (store),
	                                 NAME_COLUMN,
	                                 compare_theme,
	                                 NULL, NULL);
	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (store),
	                                      NAME_COLUMN,
	                                      CTK_SORT_ASCENDING);

	ctk_tree_view_set_row_separator_func (CTK_TREE_VIEW (tree),
	                                      separator_func,
	                                      GINT_TO_POINTER (ID_COLUMN),
	                                      NULL);

	select = ctk_tree_view_get_selection (CTK_TREE_VIEW (tree));
	ctk_tree_selection_set_mode (select, CTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (tree_selection_changed_cb),
	                  preview);

}

static void
setup_treeview_selection (CtkWidget *tree)
{
	char         *theme;
	CtkTreeModel *model;
	CtkTreeIter   iter;
	CtkTreePath  *path = NULL;
	gboolean      is_writable;

	theme = config_get_theme (&is_writable);

	if (! is_writable)
	{
		ctk_widget_set_sensitive (tree, FALSE);
	}

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (tree));

	if (theme && ctk_tree_model_get_iter_first (model, &iter))
	{

		do
		{
			char *id;
			gboolean found;

			ctk_tree_model_get (model, &iter,
			                    ID_COLUMN, &id, -1);
			found = (id && strcmp (id, theme) == 0);
			g_free (id);

			if (found)
			{
				path = ctk_tree_model_get_path (model, &iter);
				break;
			}

		}
		while (ctk_tree_model_iter_next (model, &iter));
	}

	if (path)
	{
		ctk_tree_view_set_cursor (CTK_TREE_VIEW (tree),
		                          path,
		                          NULL,
		                          FALSE);

		ctk_tree_path_free (path);
	}

	g_free (theme);
}

static void
reload_themes (void)
{
	CtkWidget    *treeview;
	CtkTreeModel *model;

	treeview = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));
	model = ctk_tree_view_get_model (CTK_TREE_VIEW (treeview));
	ctk_tree_store_clear (CTK_TREE_STORE (model));
	populate_model (CTK_TREE_STORE (model));

	ctk_tree_view_set_model (CTK_TREE_VIEW (treeview),
	                         CTK_TREE_MODEL (model));
}

static void
theme_copy_complete_cb (CtkWidget *dialog, gpointer user_data)
{
	reload_themes ();
	ctk_widget_destroy (dialog);
}

static void
theme_installer_run (CtkWidget *prefs_dialog, GList *files)
{
	CtkWidget *copy_dialog;

	copy_dialog = copy_theme_dialog_new (files);
	g_list_foreach (files, (GFunc) (g_object_unref), NULL);
	g_list_free (files);

	ctk_window_set_transient_for (CTK_WINDOW (copy_dialog),
	                              CTK_WINDOW (prefs_dialog));
	ctk_window_set_icon_name (CTK_WINDOW (copy_dialog),
	                          "preferences-desktop-screensaver");

	g_signal_connect (copy_dialog, "complete",
	                  G_CALLBACK (theme_copy_complete_cb), NULL);

	copy_theme_dialog_begin (COPY_THEME_DIALOG (copy_dialog));
}

/* Callback issued during drag movements */
static gboolean
drag_motion_cb (CtkWidget      *widget,
                GdkDragContext *context,
                int             x,
                int             y,
                guint           time,
                gpointer        data)
{
	return FALSE;
}

/* Callback issued during drag leaves */
static void
drag_leave_cb (CtkWidget      *widget,
               GdkDragContext *context,
               guint           time,
               gpointer        data)
{
	ctk_widget_queue_draw (widget);
}

/* GIO has no version of cafe_vfs_uri_list_parse(), so copy from CafeVFS
 * and re-work to create GFiles.
**/
static GList *
uri_list_parse (const gchar *uri_list)
{
	const gchar *p, *q;
	gchar *retval;
	GFile *file;
	GList *result = NULL;

	g_return_val_if_fail (uri_list != NULL, NULL);

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	 * 2396, or even check for allowed characters - we just ignore
	 * comments and trim whitespace off the ends.  We also
	 * allow LF delimination as well as the specified CRLF.
	 */
	while (p != NULL)
	{
		if (*p != '#')
		{
			while (g_ascii_isspace (*p))
				p++;

			q = p;
			while ((*q != '\0')
			        && (*q != '\n')
			        && (*q != '\r'))
				q++;

			if (q > p)
			{
				q--;
				while (q > p
				        && g_ascii_isspace (*q))
					q--;

				retval = g_malloc (q - p + 2);
				strncpy (retval, p, q - p + 1);
				retval[q - p + 1] = '\0';

				file = g_file_new_for_uri (retval);

				g_free (retval);

				if (file != NULL)
					result = g_list_prepend (result, file);
			}
		}
		p = strchr (p, '\n');
		if (p != NULL)
			p++;
	}

	return g_list_reverse (result);
}

/* Callback issued on actual drops. Attempts to load the file dropped. */
static void
drag_data_received_cb (CtkWidget        *widget,
                       GdkDragContext   *context,
                       int               x,
                       int               y,
                       CtkSelectionData *selection_data,
                       guint             info,
                       guint             time,
                       gpointer          data)
{
	GList     *files;

	if (!(info == TARGET_URI_LIST || info == TARGET_NS_URL))
		return;

	files = uri_list_parse ((char *) ctk_selection_data_get_data (selection_data));
	if (files != NULL)
	{
		CtkWidget *prefs_dialog;

		prefs_dialog = CTK_WIDGET (ctk_builder_get_object (builder, "prefs_dialog"));
		theme_installer_run (prefs_dialog, files);
	}
}

/* Adapted from totem_time_to_string_text */
static char *
time_to_string_text (long time)
{
	char *secs, *mins, *hours, *string;
	int   sec, min, hour;

	int n, inc_len, len_minutes;

	sec = time % 60;
	time = time - sec;
	min = (time % (60 * 60)) / 60;
	time = time - (min * 60);
	hour = time / (60 * 60);

	hours = g_strdup_printf (ngettext ("%d hour",
	                                   "%d hours", hour), hour);

	mins = g_strdup_printf (ngettext ("%d minute",
	                                  "%d minutes", min), min);

	secs = g_strdup_printf (ngettext ("%d second",
	                                  "%d seconds", sec), sec);

	inc_len = strlen (g_strdup_printf (_("%s %s"),
	                  g_strdup_printf (ngettext ("%d hour",
	                                             "%d hours", 1), 1),
	                  g_strdup_printf (ngettext ("%d minute",
	                                             "%d minutes", 59), 59))) - 1;

	len_minutes = 0;

	for (n = 2; n < 60; n++)
	{
		if (n < 10)
		{
			if ((strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                                        "%d minutes", n), n), NULL)) - 2) > len_minutes)

				len_minutes = strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                                                         "%d minutes", n), n), NULL)) - 2;
		}
		else
		{
			if ((strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                                        "%d minutes", n), n), NULL)) - 3) > len_minutes)

				len_minutes = strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                                                         "%d minutes", n), n), NULL)) - 3;
		}
	}

	if ((strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                        "%d minutes", 1), 1), NULL)) - 2) > len_minutes)

		len_minutes = strlen (g_str_to_ascii (g_strdup_printf (ngettext ("%d minute",
	                                                                         "%d minutes", 1), 1), NULL)) - 2;

	if (len_minutes < 1)
		len_minutes = 1;

	if (hour > 0)
	{
		if (sec > 0)
		{
			/* hour:minutes:seconds */
			string = g_strdup_printf (_("%s %s %s"), hours, mins, secs);
		}
		else if (min > 0)
		{
			/* hour:minutes */
			string = g_strdup_printf (_("%s %s"), hours, mins);
		}
		else
		{
			/* hour */
			string = g_strdup_printf (_("%s"), hours);
		}
	}
	else if (min > 0)
	{
		if (sec > 0)
		{
			/* minutes:seconds */
			string = g_strdup_printf (_("%s %s"), mins, secs);
		}
		else
		{
			/* minutes */
			string = g_strdup_printf (_("%s"), mins);

			if (min < 10)
			{
				if (min == 1)
					while (strlen (string) != (len_minutes + inc_len + 3))
					{
						if (strlen (string) % 2 == 0)
							string = g_strconcat (string, " ", NULL);
						else
							string = g_strconcat (" " , string, NULL);
					}
				else
					while (strlen (string) != (len_minutes + inc_len))
					{
						if (strlen (string) % 2 == 0)
							string = g_strconcat (string, " ", NULL);
						else
							string = g_strconcat (" " , string, NULL);
					}
			}
			else
			{
				while (strlen (string) != (len_minutes + inc_len - 1))
				{
					if (strlen (string) % 2 == 0)
						string = g_strconcat (string, " ", NULL);
					else
						string = g_strconcat (" " , string, NULL);
				}
			}
		}
	}
	else
	{
		/* seconds */
		string = g_strdup_printf (_("%s"), secs);
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

static char *
format_value_callback_time (CtkScale *scale,
                            gdouble   value)
{
	if (value == 0)
		return g_strdup_printf (_("Never"));

	return time_to_string_text (value * 60.0);
}

static void
lock_checkbox_toggled (CtkToggleButton *button, gpointer user_data)
{
	config_set_lock (ctk_toggle_button_get_active (button));
}

static void
enabled_checkbox_toggled (CtkToggleButton *button, gpointer user_data)
{
	config_set_enabled (ctk_toggle_button_get_active (button));
}

static void
picture_filename_changed (CtkFileChooserButton *button, gpointer user_data)
{
	g_settings_set_string (screensaver_settings, "picture-filename", ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (button)));
}

static void
ui_disable_lock (gboolean disable)
{
	CtkWidget *widget;

	widget = CTK_WIDGET (ctk_builder_get_object (builder, "lock_checkbox"));
	ctk_widget_set_sensitive (widget, !disable);
	if (disable)
	{
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), FALSE);
	}
}

static void
ui_set_lock (gboolean enabled)
{
	CtkWidget *widget;
	gboolean   active;
	gboolean   lock_disabled;

	widget = CTK_WIDGET (ctk_builder_get_object (builder, "lock_checkbox"));

	active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (widget));
	if (active != enabled)
	{
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), enabled);
	}
	lock_disabled = config_get_lock_disabled ();
	ui_disable_lock (lock_disabled);
}

static void
ui_set_enabled (gboolean enabled)
{
	CtkWidget *widget;
	gboolean   active;
	gboolean   is_writable;
	gboolean   lock_disabled;

	widget = CTK_WIDGET (ctk_builder_get_object (builder, "enable_checkbox"));
	active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (widget));
	if (active != enabled)
	{
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), enabled);
	}

	widget = CTK_WIDGET (ctk_builder_get_object (builder, "lock_checkbox"));
	config_get_lock (&is_writable);
	if (is_writable)
	{
		ctk_widget_set_sensitive (widget, enabled);
	}
	lock_disabled = config_get_lock_disabled ();
	ui_disable_lock(lock_disabled);
}

static void
ui_set_delay (int delay)
{
	CtkWidget *widget;

	widget = CTK_WIDGET (ctk_builder_get_object (builder, "activate_delay_hscale"));
	ctk_range_set_value (CTK_RANGE (widget), delay);
}

static void
key_changed_cb (GSettings *settings, const gchar *key, gpointer data)
{
	if (strcmp (key, KEY_IDLE_ACTIVATION_ENABLED) == 0)
	{
			gboolean enabled;

			enabled = g_settings_get_boolean (settings, key);

			ui_set_enabled (enabled);
	}
	else if (strcmp (key, KEY_LOCK) == 0)
	{
		        gboolean enabled;

			enabled = g_settings_get_boolean (settings, key);

			ui_set_lock (enabled);
	}
	else if (strcmp (key, KEY_LOCK_DISABLE) == 0)
	{
		        gboolean disabled;

			disabled = g_settings_get_boolean (settings, key);

			ui_disable_lock (disabled);
	}
	else if (strcmp (key, KEY_THEMES) == 0)
	{
		        CtkWidget *treeview;

			treeview = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));
			setup_treeview_selection (treeview);
	}
	else if (strcmp (key, KEY_IDLE_DELAY) == 0)
	{
			int delay;

			delay = g_settings_get_int (settings, key);
                        ui_set_delay (delay);

	}
	else
	{
		/*g_warning ("Config key not handled: %s", key);*/
	}
}

static void
fullscreen_preview_previous_cb (CtkWidget *fullscreen_preview_window,
                                gpointer   user_data)
{
	CtkWidget        *treeview;
	CtkTreeSelection *selection;

	treeview = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (treeview));
	tree_selection_previous (selection);
}

static void
fullscreen_preview_next_cb (CtkWidget *fullscreen_preview_window,
                            gpointer   user_data)
{
	CtkWidget        *treeview;
	CtkTreeSelection *selection;

	treeview = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (treeview));
	tree_selection_next (selection);
}

static void
fullscreen_preview_cancelled_cb (CtkWidget *button,
                                 gpointer   user_data)
{

	CtkWidget *fullscreen_preview_area;
	CtkWidget *fullscreen_preview_window;
	CtkWidget *preview_area;
	CtkWidget *dialog;

	preview_area = CTK_WIDGET (ctk_builder_get_object (builder, "preview_area"));
	gs_job_set_widget (job, preview_area);

	fullscreen_preview_area = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_area"));
	ctk_widget_queue_draw (fullscreen_preview_area);

	fullscreen_preview_window = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_window"));
	ctk_widget_hide (fullscreen_preview_window);

	dialog = CTK_WIDGET (ctk_builder_get_object (builder, "prefs_dialog"));
	ctk_widget_show (dialog);
	ctk_window_present (CTK_WINDOW (dialog));
}

static void
fullscreen_preview_start_cb (CtkWidget *widget,
                             gpointer   user_data)
{
	CtkWidget *fullscreen_preview_area;
	CtkWidget *fullscreen_preview_window;
	CtkWidget *dialog;

	dialog = CTK_WIDGET (ctk_builder_get_object (builder, "prefs_dialog"));
	ctk_widget_hide (dialog);

	fullscreen_preview_window = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_window"));

	ctk_window_fullscreen (CTK_WINDOW (fullscreen_preview_window));
	ctk_window_set_keep_above (CTK_WINDOW (fullscreen_preview_window), TRUE);

	ctk_widget_show (fullscreen_preview_window);
	ctk_widget_grab_focus (fullscreen_preview_window);

	fullscreen_preview_area = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_area"));
	ctk_widget_queue_draw (fullscreen_preview_area);
	gs_job_set_widget (job, fullscreen_preview_area);
}

static void
constrain_list_size (CtkWidget      *widget,
                     CtkAllocation  *allocation,
                     CtkWidget      *to_size)
{
	CtkRequisition req;
	int            max_height;

	/* constrain height to be the tree height up to a max */
	max_height = (HeightOfScreen (gdk_x11_screen_get_xscreen (ctk_widget_get_screen (widget)))) / 4;

	ctk_widget_get_preferred_size (to_size, &req, NULL);
	allocation->height = MIN (req.height, max_height);
}

static void
setup_list_size_constraint (CtkWidget *widget,
                            CtkWidget *to_size)
{
	g_signal_connect (widget, "size-allocate",
	                  G_CALLBACK (constrain_list_size), to_size);
}

static gboolean
check_is_root_user (void)
{
#ifndef G_OS_WIN32
	uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
	gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */

#ifdef HAVE_GETRESUID
	if (getresuid (&ruid, &euid, &suid) != 0 ||
	        getresgid (&rgid, &egid, &sgid) != 0)
#endif /* HAVE_GETRESUID */
	{
		suid = ruid = getuid ();
		sgid = rgid = getgid ();
		euid = geteuid ();
		egid = getegid ();
	}

	if (ruid == 0)
	{
		return TRUE;
	}

#endif
	return FALSE;
}

static void
setup_for_root_user (void)
{
	CtkWidget *lock_checkbox;
	CtkWidget *label;

	lock_checkbox = CTK_WIDGET (ctk_builder_get_object (builder, "lock_checkbox"));
	label = CTK_WIDGET (ctk_builder_get_object (builder, "root_warning_label"));

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (lock_checkbox), FALSE);
	ctk_widget_set_sensitive (lock_checkbox, FALSE);

	ctk_widget_show (label);
}

/* copied from gs-window-x11.c */
#ifndef _GNU_SOURCE
extern char **environ;
#endif

static gchar **
spawn_make_environment_for_display (GdkDisplay *display,
                                    gchar     **envp)
{
	gchar **retval = NULL;
	const gchar *display_name;
	gint    display_index = -1;
	gint    i, env_len;

	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

	if (envp == NULL)
		envp = environ;

	for (env_len = 0; envp[env_len]; env_len++)
		if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
			display_index = env_len;

	retval = g_new (char *, env_len + 1);
	retval[env_len] = NULL;

	display_name = gdk_display_get_name (display);

	for (i = 0; i < env_len; i++)
		if (i == display_index)
			retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
		else
			retval[i] = g_strdup (envp[i]);

	g_assert (i == env_len);

	return retval;
}

static gboolean
spawn_command_line_on_display_sync (GdkDisplay  *display,
                                    const gchar  *command_line,
                                    char        **standard_output,
                                    char        **standard_error,
                                    int          *exit_status,
                                    GError      **error)
{
	char     **argv = NULL;
	char     **envp = NULL;
	gboolean   retval;

	g_return_val_if_fail (command_line != NULL, FALSE);

	if (! g_shell_parse_argv (command_line, NULL, &argv, error))
	{
		return FALSE;
	}

	envp = spawn_make_environment_for_display (display, NULL);

	retval = g_spawn_sync (NULL,
	                       argv,
	                       envp,
	                       G_SPAWN_SEARCH_PATH,
	                       NULL,
	                       NULL,
	                       standard_output,
	                       standard_error,
	                       exit_status,
	                       error);

	g_strfreev (argv);
	g_strfreev (envp);

	return retval;
}


static GdkVisual *
get_best_visual_for_display (GdkDisplay *display)
{
	GdkScreen    *screen;
	char         *command;
	char         *std_output;
	int           exit_status;
	GError       *error;
	unsigned long v;
	char          c;
	GdkVisual    *visual;
	gboolean      res;

	visual = NULL;
	screen = gdk_display_get_default_screen (display);

	command = g_build_filename (LIBEXECDIR, "cafe-screensaver-gl-helper", NULL);

	error = NULL;
	std_output = NULL;
	res = spawn_command_line_on_display_sync (display,
	        command,
	        &std_output,
	        NULL,
	        &exit_status,
	        &error);
	if (! res)
	{
		gs_debug ("Could not run command '%s': %s", command, error->message);
		g_error_free (error);
		goto out;
	}

	if (1 == sscanf (std_output, "0x%lx %c", &v, &c))
	{
		if (v != 0)
		{
			VisualID      visual_id;

			visual_id = (VisualID) v;
			visual = gdk_x11_screen_lookup_visual (screen, visual_id);

			gs_debug ("Found best GL visual for display %s: 0x%x",
			          gdk_display_get_name (display),
			          (unsigned int) visual_id);
		}
	}
out:
	g_free (std_output);
	g_free (command);

	return visual;
}


static void
widget_set_best_visual (CtkWidget *widget)
{
	GdkVisual *visual;

	g_return_if_fail (widget != NULL);

	visual = get_best_visual_for_display (ctk_widget_get_display (widget));
	if (visual != NULL)
	{
		ctk_widget_set_visual (widget, visual);
		g_object_unref (visual);
	}
}

static gboolean
setup_treeview_idle (gpointer data)
{
	CtkWidget *preview;
	CtkWidget *treeview;

	preview  = CTK_WIDGET (ctk_builder_get_object (builder, "preview_area"));
	treeview = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));

	setup_treeview (treeview, preview);
	setup_treeview_selection (treeview);

	return FALSE;
}

static gboolean
is_program_in_path (const char *program)
{
	char *tmp = g_find_program_in_path (program);
	if (tmp != NULL)
	{
		g_free (tmp);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static void
init_capplet (void)
{
	CtkWidget *dialog;
	CtkWidget *preview;
	CtkWidget *treeview;
	CtkWidget *list_scroller;
	CtkWidget *activate_delay_hscale;
	CtkWidget *activate_delay_hbox;
	CtkWidget *label;
	CtkWidget *enabled_checkbox;
	CtkWidget *lock_checkbox;
	CtkWidget *root_warning_label;
	CtkWidget *preview_button;
	CtkWidget *gpm_button;
	CtkWidget *fullscreen_preview_window;
	CtkWidget *fullscreen_preview_area;
	CtkWidget *fullscreen_preview_previous;
	CtkWidget *fullscreen_preview_next;
	CtkWidget *fullscreen_preview_close;
	CtkWidget *picture_filename;
	gdouble    activate_delay;
	gboolean   enabled;
	gboolean   is_writable;
	GError    *error=NULL;
	gint       mode;

	builder = ctk_builder_new();
	if (!ctk_builder_add_from_resource (builder, "/org/cafe/screensaver/preferences.ui", &error))
	{
		g_warning("Couldn't load builder resource: %s", error->message);
		g_error_free(error);
	}

	if (builder == NULL)
	{

		dialog = ctk_message_dialog_new (NULL,
		                                 0, CTK_MESSAGE_ERROR,
		                                 CTK_BUTTONS_OK,
		                                 _("Could not load the main interface"));
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
		        _("Please make sure that the screensaver is properly installed"));

		ctk_dialog_set_default_response (CTK_DIALOG (dialog),
		                                 CTK_RESPONSE_OK);
		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		exit (1);
	}

	preview            = CTK_WIDGET (ctk_builder_get_object (builder, "preview_area"));
	dialog             = CTK_WIDGET (ctk_builder_get_object (builder, "prefs_dialog"));
	treeview           = CTK_WIDGET (ctk_builder_get_object (builder, "savers_treeview"));
	list_scroller      = CTK_WIDGET (ctk_builder_get_object (builder, "themes_scrolled_window"));
	activate_delay_hscale = CTK_WIDGET (ctk_builder_get_object (builder, "activate_delay_hscale"));
	activate_delay_hbox   = CTK_WIDGET (ctk_builder_get_object (builder, "activate_delay_hbox"));
	enabled_checkbox   = CTK_WIDGET (ctk_builder_get_object (builder, "enable_checkbox"));
	lock_checkbox      = CTK_WIDGET (ctk_builder_get_object (builder, "lock_checkbox"));
	root_warning_label = CTK_WIDGET (ctk_builder_get_object (builder, "root_warning_label"));
	preview_button     = CTK_WIDGET (ctk_builder_get_object (builder, "preview_button"));
	gpm_button         = CTK_WIDGET (ctk_builder_get_object (builder, "gpm_button"));
	fullscreen_preview_window = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_window"));
	fullscreen_preview_area = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_area"));
	fullscreen_preview_close = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_close"));
	fullscreen_preview_previous = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_previous_button"));
	fullscreen_preview_next = CTK_WIDGET (ctk_builder_get_object (builder, "fullscreen_preview_next_button"));
	picture_filename = CTK_WIDGET (ctk_builder_get_object (builder, "picture_filename"));

	label              = CTK_WIDGET (ctk_builder_get_object (builder, "activate_delay_label"));
	ctk_label_set_mnemonic_widget (CTK_LABEL (label), activate_delay_hscale);
	label              = CTK_WIDGET (ctk_builder_get_object (builder, "savers_label"));
	ctk_label_set_mnemonic_widget (CTK_LABEL (label), treeview);

	ctk_widget_set_no_show_all (root_warning_label, TRUE);
	widget_set_best_visual (preview);

	if (! is_program_in_path (GPM_COMMAND))
	{
		ctk_widget_set_no_show_all (gpm_button, TRUE);
		ctk_widget_hide (gpm_button);
	}

	screensaver_settings = g_settings_new (GSETTINGS_SCHEMA);
	g_signal_connect (screensaver_settings,
	                  "changed",
	                  G_CALLBACK (key_changed_cb),
	                  NULL);

	session_settings = g_settings_new (SESSION_SETTINGS_SCHEMA);
	g_signal_connect (session_settings,
	                  "changed::" KEY_IDLE_DELAY,
	                  G_CALLBACK (key_changed_cb),
	                  NULL);

	lockdown_settings = g_settings_new (LOCKDOWN_SETTINGS_SCHEMA);
	g_signal_connect (lockdown_settings,
	                  "changed::" KEY_LOCK_DISABLE,
	                  G_CALLBACK (key_changed_cb),
	                  NULL);

	activate_delay = config_get_activate_delay (&is_writable);
	ui_set_delay (activate_delay);
	if (! is_writable)
	{
		ctk_widget_set_sensitive (activate_delay_hbox, FALSE);
	}
	g_signal_connect (activate_delay_hscale, "format-value",
	                  G_CALLBACK (format_value_callback_time), NULL);

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (lock_checkbox), config_get_lock (&is_writable));
	if (! is_writable)
	{
		ctk_widget_set_sensitive (lock_checkbox, FALSE);
	}
	g_signal_connect (lock_checkbox, "toggled",
	                  G_CALLBACK (lock_checkbox_toggled), NULL);

	char *path = g_settings_get_string (screensaver_settings, "picture-filename");
	ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (ctk_builder_get_object (builder, "picture_filename")), path);
	ctk_file_filter_add_pixbuf_formats (CTK_FILE_FILTER (ctk_builder_get_object (builder, "picture_filefilter")));
	g_free (path);
	g_signal_connect (picture_filename, "selection-changed",
	                  G_CALLBACK (picture_filename_changed), NULL);

	enabled = config_get_enabled (&is_writable);
	ui_set_enabled (enabled);
	if (! is_writable)
	{
		ctk_widget_set_sensitive (enabled_checkbox, FALSE);
	}
	g_signal_connect (enabled_checkbox, "toggled",
	                  G_CALLBACK (enabled_checkbox_toggled), NULL);

	setup_list_size_constraint (list_scroller, treeview);
	ctk_widget_set_size_request (preview, 480, 300);
	ctk_window_set_icon_name (CTK_WINDOW (dialog), "preferences-desktop-screensaver");
	ctk_window_set_icon_name (CTK_WINDOW (fullscreen_preview_window), "screensaver");

	g_signal_connect (fullscreen_preview_area,
	                  "draw", G_CALLBACK (preview_on_draw),
	                  NULL);

	ctk_drag_dest_set (dialog, CTK_DEST_DEFAULT_ALL,
	                   drop_types, G_N_ELEMENTS (drop_types),
	                   GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

	g_signal_connect (dialog, "drag-motion",
	                  G_CALLBACK (drag_motion_cb), NULL);
	g_signal_connect (dialog, "drag-leave",
	                  G_CALLBACK (drag_leave_cb), NULL);
	g_signal_connect (dialog, "drag-data-received",
	                  G_CALLBACK (drag_data_received_cb), NULL);

	ctk_widget_show_all (dialog);

	/* Update list of themes if using random screensaver */
	mode = g_settings_get_enum (screensaver_settings, KEY_MODE);
	if (mode == GS_MODE_RANDOM) {
		gchar **list;
		list = get_all_theme_ids (theme_manager);
		g_settings_set_strv (screensaver_settings, KEY_THEMES, (const gchar * const*) list);
		g_strfreev (list);
	}

	g_signal_connect (preview, "draw", G_CALLBACK (preview_on_draw), NULL);
	gs_job_set_widget (job, preview);

	if (check_is_root_user ())
	{
		setup_for_root_user ();
	}

	g_signal_connect (activate_delay_hscale, "value-changed",
	                  G_CALLBACK (activate_delay_value_changed_cb), NULL);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (response_cb), NULL);

	g_signal_connect (preview_button, "clicked",
	                  G_CALLBACK (fullscreen_preview_start_cb),
	                  treeview);

	g_signal_connect (fullscreen_preview_close, "clicked",
	                  G_CALLBACK (fullscreen_preview_cancelled_cb), NULL);
	g_signal_connect (fullscreen_preview_previous, "clicked",
	                  G_CALLBACK (fullscreen_preview_previous_cb), NULL);
	g_signal_connect (fullscreen_preview_next, "clicked",
	                  G_CALLBACK (fullscreen_preview_next_cb), NULL);

	g_idle_add ((GSourceFunc)setup_treeview_idle, NULL);
}

static void
finalize_capplet (void)
{
	g_object_unref (screensaver_settings);
	g_object_unref (session_settings);
	g_object_unref (lockdown_settings);
}

int
main (int    argc,
      char **argv)
{

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
	textdomain (GETTEXT_PACKAGE);
#endif

	ctk_init (&argc, &argv);

	job = gs_job_new ();
	theme_manager = gs_theme_manager_new ();

	init_capplet ();

	ctk_main ();

	finalize_capplet ();

	g_object_unref (theme_manager);
	g_object_unref (job);

	return 0;
}
