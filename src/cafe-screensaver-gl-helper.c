/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
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
#include <glib.h>
#include <glib/gi18n.h>

#include <ctk/ctk.h>
#include <cdk/cdkx.h>

#include "gs-visual-gl.h"

int
main (int    argc,
      char **argv)
{
	CdkDisplay     *display;
	CdkVisual      *visual;
	Visual         *xvisual;
	GError         *error = NULL;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
	textdomain (GETTEXT_PACKAGE);
#endif

	g_set_prgname (argv[0]);
	if (! ctk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error))
	{
		if (error != NULL)
		{
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		else
		{
			g_warning ("Unable to initialize CTK+");
		}
		exit (1);
	}

	display = cdk_display_get_default ();
	visual = gs_visual_gl_get_best_for_display (display);

	if (visual != NULL)
	{
		xvisual = cdk_x11_visual_get_xvisual (visual);
		printf ("0x%x\n", (unsigned int) XVisualIDFromVisual (xvisual));
	}
	else
	{
		printf ("none\n");
	}

	return 0;
}
