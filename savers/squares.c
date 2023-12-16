/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctk/ctk.h>

#include <glib/gi18n.h>

#include "gs-theme-window.h"
#include "gs-theme-engine.h"
#include "gste-popsquares.h"

int
main (int argc, char **argv)
{
	GSThemeEngine *engine;
	CtkWidget     *window;
	GError        *error;

	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	error = NULL;

	if (!ctk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error))
	{
		g_printerr (_("%s. See --help for usage information.\n"),
		            error->message);
		g_error_free (error);
		exit (1);
	}

	window = gs_theme_window_new ();
	g_signal_connect (G_OBJECT (window), "delete-event",
	                  G_CALLBACK (ctk_main_quit), NULL);

	g_set_prgname ("popsquares");

	engine = g_object_new (GSTE_TYPE_POPSQUARES, NULL);
	ctk_container_add (CTK_CONTAINER (window), CTK_WIDGET (engine));

	ctk_widget_show (CTK_WIDGET (engine));

	ctk_window_set_default_size (CTK_WINDOW (window), 640, 480);
	ctk_widget_show (window);

	ctk_main ();

	return 0;
}
