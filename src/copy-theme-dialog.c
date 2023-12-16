/* copy-theme-dialog.c
 * Copyright (C) 2008 John Millikin <jmillikin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
**/

#include "config.h"

#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <gio/gio.h>

#include "copy-theme-dialog.h"

static void
add_file_to_dialog (gpointer data, gpointer user_data);
static void
single_copy_complete (GObject *source_object, GAsyncResult *res,
                      gpointer user_data);
static void
copy_theme_dialog_copy_next (CopyThemeDialog *dialog);
static void
copy_theme_dialog_cancel (CopyThemeDialog *dialog);
static void
copy_theme_dialog_finalize (GObject *obj);
static void
copy_theme_dialog_update_num_files (CopyThemeDialog *dlg);
static void
copy_theme_dialog_response (CtkDialog *dialog, gint response_id);
static void
eel_ctk_label_make_bold (CtkLabel *label);
static void
create_titled_label (CtkGrid    *grid,
                     int         row,
                     CtkWidget **title_widget,
                     CtkWidget **label_text_widget);

static GObjectClass *parent_class = NULL;

enum
{
    CANCELLED = 0,
    COMPLETE,
    SIGNAL_COUNT
};

struct _CopyThemeDialogPrivate
{
	CtkWidget *progress;
	CtkWidget *status;
	CtkWidget *current;
	CtkWidget *from;
	CtkWidget *to;

	GFile *theme_dir;
	GSList *all_files, *file;
	GSList *all_basenames, *basename;
	guint index;
	guint total_files;
	GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (CopyThemeDialog, copy_theme_dialog, CTK_TYPE_DIALOG)

guint signals[SIGNAL_COUNT] = {0, 0};

static void
copy_theme_dialog_class_init (CopyThemeDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	klass->cancelled = copy_theme_dialog_cancel;
	object_class->finalize = copy_theme_dialog_finalize;

	CTK_DIALOG_CLASS (klass)->response = copy_theme_dialog_response;

	signals[CANCELLED] = g_signal_new ("cancelled",
	                                   G_TYPE_FROM_CLASS (object_class),
	                                   G_SIGNAL_RUN_FIRST,
	                                   G_STRUCT_OFFSET (CopyThemeDialogClass, cancelled),
	                                   NULL, NULL,
	                                   g_cclosure_marshal_VOID__VOID,
	                                   G_TYPE_NONE, 0);

	signals[COMPLETE] = g_signal_new ("complete",
	                                  G_TYPE_FROM_CLASS (object_class),
	                                  G_SIGNAL_RUN_LAST,
	                                  G_STRUCT_OFFSET (CopyThemeDialogClass, complete),
	                                  NULL, NULL,
	                                  g_cclosure_marshal_VOID__VOID,
	                                  G_TYPE_NONE, 0);

	parent_class = g_type_class_peek_parent (klass);
}

CtkWidget*
copy_theme_dialog_new (GList *files)
{
	CtkWidget *dialog;
	CopyThemeDialogPrivate *priv;

	dialog = CTK_WIDGET (g_object_new (COPY_THEME_DIALOG_TYPE, NULL));
	priv = COPY_THEME_DIALOG (dialog)->priv;
	priv->index = 0;
	priv->total_files = 0;
	priv->all_files = NULL;
	priv->all_basenames = NULL;

	g_list_foreach (files, add_file_to_dialog, dialog);

	priv->file = priv->all_files;
	priv->basename = priv->all_basenames;

	return dialog;
}

static gboolean
copy_finished (CopyThemeDialog *dialog)
{
	return (g_cancellable_is_cancelled (dialog->priv->cancellable) ||
	        dialog->priv->file == NULL);
}

static void
copy_theme_dialog_init (CopyThemeDialog *dlg)
{
	CtkWidget *vbox;
	CtkWidget *hbox;
	CtkWidget *progress_vbox;
	CtkWidget *grid;
	CtkWidget *label;
	CtkWidget *dialog_vbox;
	char      *markup;
	gchar     *theme_dir_path;

	dlg->priv = copy_theme_dialog_get_instance_private (dlg);

	/* Find and, if needed, create the directory for storing themes */
	theme_dir_path = g_build_filename (g_get_user_data_dir (),
	                                   "applications", "screensavers",
	                                   NULL);
	dlg->priv->theme_dir = g_file_new_for_path (theme_dir_path);
	g_mkdir_with_parents (theme_dir_path, S_IRWXU);
	g_free (theme_dir_path);

	/* For cancelling async I/O operations */
	dlg->priv->cancellable = g_cancellable_new ();

	/* GUI settings */
	dialog_vbox = ctk_dialog_get_content_area (CTK_DIALOG (dlg));

	ctk_container_set_border_width (CTK_CONTAINER (dialog_vbox),
	                                4);
	ctk_box_set_spacing (CTK_BOX (dialog_vbox), 4);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_container_set_border_width (CTK_CONTAINER (vbox), 6);
	ctk_box_pack_start (CTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

	dlg->priv->status = ctk_label_new ("");
	markup = g_strdup_printf ("<big><b>%s</b></big>", _("Copying files"));
	ctk_label_set_markup (CTK_LABEL (dlg->priv->status), markup);
	g_free (markup);

	ctk_widget_set_halign (dlg->priv->status, CTK_ALIGN_START);
	ctk_widget_set_valign (dlg->priv->status, CTK_ALIGN_START);
	ctk_box_pack_start (CTK_BOX (vbox), dlg->priv->status, FALSE, FALSE, 0);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_box_pack_start (CTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	grid = ctk_grid_new ();
	ctk_grid_set_row_spacing (CTK_GRID (grid), 4);
	ctk_grid_set_column_spacing (CTK_GRID (grid), 4);
	ctk_grid_set_column_homogeneous (CTK_GRID (grid), TRUE);

	create_titled_label (CTK_GRID (grid), 0,
	                     &label,
	                     &dlg->priv->from);
	ctk_label_set_text (CTK_LABEL (label), _("From:"));
	create_titled_label (CTK_GRID (grid), 1,
	                     &label,
	                     &dlg->priv->to);
	ctk_label_set_text (CTK_LABEL (label), _("To:"));

	ctk_box_pack_start (CTK_BOX (vbox), CTK_WIDGET (grid), FALSE, FALSE, 0);

	progress_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_box_set_homogeneous (CTK_BOX (progress_vbox), TRUE);
	ctk_box_pack_start (CTK_BOX (vbox), progress_vbox, FALSE, FALSE, 0);

	dlg->priv->progress = ctk_progress_bar_new ();
	ctk_box_pack_start (CTK_BOX (progress_vbox),
	                    dlg->priv->progress, FALSE, FALSE, 0);

	dlg->priv->current = ctk_label_new ("");
	ctk_box_pack_start (CTK_BOX (progress_vbox),
	                    dlg->priv->current, FALSE, FALSE, 0);
	ctk_widget_set_halign (dlg->priv->current, CTK_ALIGN_START);

	ctk_dialog_add_button (CTK_DIALOG (dlg),
	                       "ctk-cancel", CTK_RESPONSE_CANCEL);

	ctk_window_set_title (CTK_WINDOW (dlg),
	                      _("Copying themes"));
	ctk_container_set_border_width (CTK_CONTAINER (dlg), 6);

	ctk_widget_show_all (dialog_vbox);
}

static void
add_file_to_dialog (gpointer data, gpointer user_data)
{
	CopyThemeDialogPrivate *priv;
	GFile *file;
	gchar *basename = NULL, *raw_basename;

	priv = COPY_THEME_DIALOG (user_data)->priv;
	file = G_FILE (data);

	raw_basename = g_file_get_basename (file);
	if (g_str_has_suffix (raw_basename, ".desktop"))
	{
		/* FIXME: validate key file? */
		basename = g_strndup (raw_basename,
		                      /* 8 = strlen (".desktop") */
		                      strlen (raw_basename) - 8);
	}
	g_free (raw_basename);

	if (basename)
	{
		g_object_ref (file);
		priv->all_files = g_slist_append (priv->all_files, file);
		priv->all_basenames = g_slist_append (priv->all_basenames, basename);
		priv->total_files++;
	}

	else
	{
		CtkWidget *dialog;
		gchar *uri;

		dialog = ctk_message_dialog_new (CTK_WINDOW (user_data),
		                                 CTK_DIALOG_MODAL,
		                                 CTK_MESSAGE_ERROR,
		                                 CTK_BUTTONS_OK,
		                                 _("Invalid screensaver theme"));
		uri = g_file_get_uri (file);
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
		        _("%s does not appear to be a valid screensaver theme."),
		        uri);
		g_free (uri);
		ctk_window_set_title (CTK_WINDOW (dialog), "");
		ctk_window_set_icon_name (CTK_WINDOW (dialog), "preferences-desktop-screensaver");

		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
	}
}

static void
single_copy_complete (GObject *source_object, GAsyncResult *res,
                      gpointer user_data)
{
	GError *error = NULL;
	gboolean should_continue = FALSE;
	CopyThemeDialog *dialog = COPY_THEME_DIALOG (user_data);

	if (g_file_copy_finish (G_FILE (source_object), res, &error))
	{
		should_continue = TRUE;
	}

	else
	{
		/* If the file already exists, generate a new random name
		 * and try again.
		**/
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
		{
			GFile *file, *destination;
			gchar *basename, *full_basename;
			g_error_free (error);

			file = G_FILE (dialog->priv->file->data);
			basename = (gchar *) (dialog->priv->basename->data);

			g_return_if_fail (file != NULL);
			g_return_if_fail (basename != NULL);

			full_basename = g_strdup_printf ("%s-%u.desktop",
			                                 basename,
			                                 g_random_int ());
			destination = g_file_get_child (dialog->priv->theme_dir,
			                                full_basename);
			g_free (full_basename);

			g_file_copy_async (file, destination, G_FILE_COPY_NONE,
			                   G_PRIORITY_DEFAULT,
			                   dialog->priv->cancellable,
			                   NULL, NULL,
			                   single_copy_complete, dialog);
		}

		else
		{
			if (g_error_matches (error, G_IO_ERROR,
			                     G_IO_ERROR_CANCELLED))
			{
				/* User has cancelled the theme copy */
				g_signal_emit (G_OBJECT (dialog),
				               signals[CANCELLED],
				               0, NULL);
			}

			else
			{
				/* Some other error occurred, ignore and
				 * try to copy remaining files
				**/
				should_continue = TRUE;
			}

			g_error_free (error);
		}
	}

	/* Update informational widgets and, if needed, signal
	 * copy completion.
	**/
	if (should_continue)
	{
		dialog->priv->index++;
		dialog->priv->file = dialog->priv->file->next;
		dialog->priv->basename = dialog->priv->basename->next;
		copy_theme_dialog_update_num_files (dialog);
		copy_theme_dialog_copy_next (dialog);
	}
}

/* Try to copy the theme file to the user's screensaver directory.
 * If a file with the given name already exists, the error will be
 * caught later and the copy re-attempted with a random value
 * appended to the filename.
**/
static void
copy_theme_dialog_copy_next (CopyThemeDialog *dialog)
{
	GFile *file, *destination;
	gchar *basename, *full_basename;

	if (copy_finished (dialog))
	{
		g_signal_emit (G_OBJECT (dialog), signals[COMPLETE],
		               0, NULL);
		return;
	}

	file = G_FILE (dialog->priv->file->data);
	basename = (gchar *) (dialog->priv->basename->data);

	g_return_if_fail (file != NULL);
	g_return_if_fail (basename != NULL);

	full_basename = g_strdup_printf ("%s.desktop", basename);
	destination = g_file_get_child (dialog->priv->theme_dir, full_basename);
	g_free (full_basename);

	g_file_copy_async (file, destination, G_FILE_COPY_NONE,
	                   G_PRIORITY_DEFAULT, dialog->priv->cancellable,
	                   NULL, NULL, single_copy_complete, dialog);
}

static gboolean
timeout_display_dialog (gpointer data)
{
	if (IS_COPY_THEME_DIALOG (data))
	{
		CopyThemeDialog *dialog = COPY_THEME_DIALOG (data);
		if (!copy_finished (dialog))
		{
			ctk_widget_show (CTK_WIDGET (dialog));

			g_signal_connect (dialog, "response",
			                  G_CALLBACK (copy_theme_dialog_response),
			                  dialog);
		}
	}
	return FALSE;
}

void
copy_theme_dialog_begin (CopyThemeDialog *dialog)
{
	ctk_widget_hide (CTK_WIDGET (dialog));

	/* If the copy operation takes more than half a second to
	 * complete, display the dialog.
	**/
	g_timeout_add (500, timeout_display_dialog, dialog);

	copy_theme_dialog_copy_next (dialog);
}

static void
copy_theme_dialog_cancel (CopyThemeDialog *dialog)
{
	g_cancellable_cancel (dialog->priv->cancellable);
}

static void
copy_theme_dialog_finalize (GObject *obj)
{
	CopyThemeDialog *dlg = COPY_THEME_DIALOG (obj);

	g_object_unref (dlg->priv->theme_dir);
	g_slist_foreach (dlg->priv->all_files, (GFunc) (g_object_unref), NULL);
	g_slist_free (dlg->priv->all_files);
	g_slist_foreach (dlg->priv->all_basenames, (GFunc) (g_free), NULL);
	g_slist_free (dlg->priv->all_basenames);
	g_object_unref (dlg->priv->cancellable);

	if (parent_class->finalize)
		parent_class->finalize (G_OBJECT (dlg));
}

static void
copy_theme_dialog_update_num_files (CopyThemeDialog *dlg)
{
	gchar *str = g_strdup_printf (_("Copying file: %u of %u"),
	                              dlg->priv->index, dlg->priv->total_files);
	ctk_progress_bar_set_text (CTK_PROGRESS_BAR (dlg->priv->progress), str);
	g_free (str);
}

static void
copy_theme_dialog_response (CtkDialog *dialog, gint response_id)
{
	g_cancellable_cancel (COPY_THEME_DIALOG (dialog)->priv->cancellable);
}

/**
 * eel_ctk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/
static void
eel_ctk_label_make_bold (CtkLabel *label)
{
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_new ();

	pango_font_description_set_weight (font_desc,
	                                   PANGO_WEIGHT_BOLD);

	/* This will only affect the weight of the font, the rest is
	 * from the current state of the widget, which comes from the
	 * theme or user prefs, since the font desc only has the
	 * weight flag turned on.
	 */
	ctk_widget_override_font (CTK_WIDGET (label), font_desc);
	pango_font_description_free (font_desc);
}

/* from caja */
static void
create_titled_label (CtkGrid    *grid,
                     int         row,
                     CtkWidget **title_widget,
                     CtkWidget **label_text_widget)
{
	*title_widget = ctk_label_new ("");
	eel_ctk_label_make_bold (CTK_LABEL (*title_widget));
	ctk_widget_set_halign (*title_widget, CTK_ALIGN_END);
	ctk_widget_set_valign (*title_widget, CTK_ALIGN_START);

	ctk_grid_attach (grid, *title_widget,
	                 0, row, 1, 1);
	ctk_widget_show (*title_widget);

	*label_text_widget = ctk_label_new ("");
	ctk_label_set_ellipsize (CTK_LABEL (*label_text_widget), PANGO_ELLIPSIZE_END);
	ctk_widget_set_hexpand (*label_text_widget, TRUE);
	ctk_grid_attach (grid, *label_text_widget,
	                 1, row, 1, 1);
	ctk_widget_show (*label_text_widget);
	ctk_widget_set_halign (*label_text_widget, CTK_ALIGN_START);
	ctk_widget_set_valign (*label_text_widget, CTK_ALIGN_START);
}
