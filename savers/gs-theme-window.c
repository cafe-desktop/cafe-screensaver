/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * gs-theme-window.c - special toplevel for screensavers
 *
 * Copyright (C) 2005 Ray Strode <rstrode@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Originally written by: Ray Strode <rstrode@redhat.com>
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <cdk/cdkx.h>
#include <ctk/ctk.h>

#include "gs-theme-window.h"

static void gs_theme_window_finalize     (GObject *object);
static void gs_theme_window_real_realize (CtkWidget *widget);

static GObjectClass   *parent_class = NULL;

G_DEFINE_TYPE (GSThemeWindow, gs_theme_window, CTK_TYPE_WINDOW)

#define MIN_SIZE 10

static void
gs_theme_window_class_init (GSThemeWindowClass *klass)
{
	GObjectClass   *object_class;
	CtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = CTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gs_theme_window_finalize;

	widget_class->realize = gs_theme_window_real_realize;
}

static void
gs_theme_window_init (GSThemeWindow *window)
{
	ctk_widget_set_app_paintable (CTK_WIDGET (window), TRUE);
}

static void
gs_theme_window_finalize (GObject *object)
{
	GObjectClass  *parent_class;

	GS_THEME_WINDOW (object);

	parent_class = G_OBJECT_CLASS (gs_theme_window_parent_class);

	if (parent_class->finalize != NULL)
		parent_class->finalize (object);
}

static void
gs_theme_window_real_realize (CtkWidget *widget)
{
	GdkWindow     *window;
	Window         remote_xwindow;
	CtkRequisition requisition;
	CtkAllocation  allocation;
	const char    *preview_xid;
	int            x;
	int            y;
	int            width;
	int            height;
	int            event_mask;

	event_mask = 0;
	window = NULL;
	preview_xid = g_getenv ("XSCREENSAVER_WINDOW");

	if (preview_xid != NULL)
	{
		char *end;

		remote_xwindow = (Window) strtoul (preview_xid, &end, 0);

		if ((remote_xwindow != 0) && (end != NULL) &&
		        ((*end == ' ') || (*end == '\0')) &&
		        ((remote_xwindow < G_MAXULONG) || (errno != ERANGE)))
		{
			window = cdk_x11_window_foreign_new_for_display (cdk_display_get_default (), remote_xwindow);
			if (window != NULL)
			{
				/* This is a kludge; we need to set the same
				 * flags gs-window-x11.c does, to ensure they
				 * don't get unset by ctk_window_map() later.
				 */
				ctk_window_set_decorated (CTK_WINDOW (widget), FALSE);

				ctk_window_set_skip_taskbar_hint (CTK_WINDOW (widget), TRUE);
				ctk_window_set_skip_pager_hint (CTK_WINDOW (widget), TRUE);

				ctk_window_set_keep_above (CTK_WINDOW (widget), TRUE);

				ctk_window_fullscreen (CTK_WINDOW (widget));

				event_mask = CDK_EXPOSURE_MASK | CDK_STRUCTURE_MASK;
				ctk_widget_set_events (widget, ctk_widget_get_events (widget) | event_mask);
			}
		}
	}

	if (window == NULL)
	{
		CtkWidgetClass *parent_class;

		parent_class = CTK_WIDGET_CLASS (gs_theme_window_parent_class);

		if (parent_class->realize != NULL)
			parent_class->realize (widget);

		return;
	}

	ctk_style_context_set_background (ctk_widget_get_style_context (widget),
	                                  window);
	cdk_window_set_decorations (window, (GdkWMDecoration) 0);
	cdk_window_set_events (window, cdk_window_get_events (window) | event_mask);

	ctk_widget_set_window (widget, window);
	cdk_window_set_user_data (window, widget);
	ctk_widget_set_realized (widget, TRUE);

	cdk_window_get_geometry (window, &x, &y, &width, &height);

	if (width < MIN_SIZE || height < MIN_SIZE)
	{
		g_critical ("This window is way too small to use");
		exit (1);
	}

	ctk_widget_get_preferred_size (widget, &requisition, NULL);
	allocation.x = x;
	allocation.y = y;
	allocation.width = width;
	allocation.height = height;
	ctk_widget_size_allocate (widget, &allocation);
	ctk_window_resize (CTK_WINDOW (widget), width, height);
}

CtkWidget *
gs_theme_window_new (void)
{
	GSThemeWindow *window;

	window = g_object_new (GS_TYPE_THEME_WINDOW,
	                       "type", CTK_WINDOW_TOPLEVEL,
	                       NULL);

	return CTK_WIDGET (window);
}
