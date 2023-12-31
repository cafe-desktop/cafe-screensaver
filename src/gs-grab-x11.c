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
 *
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <ctk/ctk.h>

#include "gs-window.h"
#include "gs-grab.h"
#include "gs-debug.h"

static void     gs_grab_finalize   (GObject     *object);

static gpointer grab_object = NULL;

struct GSGrabPrivate
{
	CdkWindow  *grab_window;
	CdkDisplay *grab_display;
	guint       no_pointer_grab : 1;
	guint       hide_cursor : 1;

	CtkWidget *invisible;
};

G_DEFINE_TYPE_WITH_PRIVATE (GSGrab, gs_grab, G_TYPE_OBJECT)

static const char *
grab_string (int status)
{
	switch (status)
	{
	case CDK_GRAB_SUCCESS:
		return "GrabSuccess";
	case CDK_GRAB_ALREADY_GRABBED:
		return "AlreadyGrabbed";
	case CDK_GRAB_INVALID_TIME:
		return "GrabInvalidTime";
	case CDK_GRAB_NOT_VIEWABLE:
		return "GrabNotViewable";
	case CDK_GRAB_FROZEN:
		return "GrabFrozen";
	case CDK_GRAB_FAILED:
		return "GrabFailed";
	default:
	{
		static char foo [255];
		sprintf (foo, "unknown status: %d", status);
		return foo;
	}
	}
}

static void
xorg_lock_smasher_set_active (GSGrab  *grab,
                              gboolean active)
{
}

static void
prepare_window_grab_cb (CdkSeat   *seat,
                        CdkWindow *window,
                        gpointer   user_data)
{
	cdk_window_show_unraised (window);
}

static int
gs_grab_get (GSGrab     *grab,
             CdkWindow  *window,
             CdkDisplay *display,
             gboolean    no_pointer_grab,
             gboolean    hide_cursor)
{
	CdkGrabStatus status;
	CdkSeat      *seat;
	CdkSeatCapabilities caps;
	CdkCursor    *cursor;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (display != NULL, FALSE);

	cursor = cdk_cursor_new_for_display (display, CDK_BLANK_CURSOR);

	gs_debug ("Grabbing devices for window=%X", (guint32) CDK_WINDOW_XID (window));

	seat = cdk_display_get_default_seat (display);
	if (!no_pointer_grab)
		caps = CDK_SEAT_CAPABILITY_ALL;
	else
		caps = CDK_SEAT_CAPABILITY_KEYBOARD;

	status = cdk_seat_grab (seat, window,
	                        caps, TRUE,
	                        (hide_cursor ? cursor : NULL),
	                        NULL,
	                        prepare_window_grab_cb,
	                        NULL);

	/* make it release grabbed pointer if requested and if any;
	   time between grabbing and ungrabbing is minimal as grab was already
	   completed once */
	if (status == CDK_GRAB_SUCCESS && no_pointer_grab &&
	    cdk_display_device_is_grabbed (display, cdk_seat_get_pointer (seat)))
	{
		gs_grab_release (grab, FALSE);
		gs_debug ("Regrabbing keyboard");
		status = cdk_seat_grab (seat, window,
		                        caps, TRUE,
		                        (hide_cursor ? cursor : NULL),
		                        NULL, NULL, NULL);
	}

	if (status == CDK_GRAB_SUCCESS)
	{
		if (grab->priv->grab_window != NULL)
		{
			g_object_remove_weak_pointer (G_OBJECT (grab->priv->grab_window),
			                              (gpointer *) &grab->priv->grab_window);
		}
		grab->priv->grab_window = window;

		g_object_add_weak_pointer (G_OBJECT (grab->priv->grab_window),
		                           (gpointer *) &grab->priv->grab_window);

		grab->priv->grab_display = display;
		grab->priv->no_pointer_grab = no_pointer_grab;
		grab->priv->hide_cursor = hide_cursor;
	}

	g_object_unref (G_OBJECT (cursor));

	return status;
}

void
gs_grab_reset (GSGrab *grab)
{
	if (grab->priv->grab_window != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (grab->priv->grab_window),
		                              (gpointer *) &grab->priv->grab_window);
	}
	grab->priv->grab_window = NULL;
	grab->priv->grab_display = NULL;
}

void
gs_grab_release (GSGrab *grab, gboolean flush)
{
	CdkDisplay *display;
	CdkSeat    *seat;

	display = cdk_display_get_default ();
	seat = cdk_display_get_default_seat (display);

	gs_debug ("Ungrabbing devices");

	cdk_seat_ungrab (seat);

	gs_grab_reset (grab);

	/* FIXME: decide when this is good and when not */
	if (flush)
	{
		/* FIXME: is it right to enable this? */
		xorg_lock_smasher_set_active (grab, TRUE);

		cdk_display_sync (display);
		cdk_display_flush (display);
	}
}

static gboolean
gs_grab_move (GSGrab     *grab,
              CdkWindow  *window,
              CdkDisplay *display,
              gboolean    no_pointer_grab,
              gboolean    hide_cursor)
{
	int         result;
	CdkWindow  *old_window;
	CdkDisplay *old_display;
	gboolean    old_hide_cursor;

	if (grab->priv->grab_window == window &&
	    grab->priv->no_pointer_grab == no_pointer_grab)
	{
		gs_debug ("Window %X is already grabbed, skipping",
		          (guint32) CDK_WINDOW_XID (grab->priv->grab_window));
		return TRUE;
	}

	if (grab->priv->grab_window != NULL)
	{
		gs_debug ("Moving devices grab from %X to %X",
		          (guint32) CDK_WINDOW_XID (grab->priv->grab_window),
		          (guint32) CDK_WINDOW_XID (window));
	}
	else
	{
		gs_debug ("Getting devices grab on %X",
		          (guint32) CDK_WINDOW_XID (window));

	}

	gs_debug ("*** doing X server grab");
	cdk_x11_display_grab (display);

	old_window = grab->priv->grab_window;
	old_display = grab->priv->grab_display;
	old_hide_cursor = grab->priv->hide_cursor;

	if (old_window)
	{
		gs_grab_release (grab, FALSE);
	}

	result = gs_grab_get (grab, window, display,
	                      no_pointer_grab, hide_cursor);

	if (result != CDK_GRAB_SUCCESS)
	{
		g_usleep (G_USEC_PER_SEC);
		result = gs_grab_get (grab, window, display,
		                      no_pointer_grab, hide_cursor);
	}

	if ((result != CDK_GRAB_SUCCESS) && old_window)
	{
		int old_result;

		gs_debug ("Could not grab devices for new window. Resuming previous grab.");
		old_result = gs_grab_get (grab, old_window, old_display,
		                          no_pointer_grab, old_hide_cursor);
		if (old_result != CDK_GRAB_SUCCESS)
			gs_debug ("Could not grab devices for old window");
	}

	gs_debug ("*** releasing X server grab");
	cdk_x11_display_ungrab (display);
	cdk_display_flush (display);

	return (result == CDK_GRAB_SUCCESS);
}

static void
gs_grab_nuke_focus (CdkDisplay *display)
{
	Window focus = 0;
	int    rev = 0;

	gs_debug ("Nuking focus");

	cdk_x11_display_error_trap_push (display);

	XGetInputFocus (CDK_DISPLAY_XDISPLAY (display), &focus, &rev);
	XSetInputFocus (CDK_DISPLAY_XDISPLAY (display), None,
	                RevertToNone, CurrentTime);

	cdk_x11_display_error_trap_pop_ignored (display);
}

gboolean
gs_grab_grab_window (GSGrab     *grab,
                     CdkWindow  *window,
                     CdkDisplay *display,
                     gboolean    no_pointer_grab,
                     gboolean    hide_cursor)
{
	gboolean    status = FALSE;
	int         i;
	int         retries = 12;

	for (i = 0; i < retries; i++)
	{
		status = gs_grab_get (grab, window, display,
		                      no_pointer_grab, hide_cursor);
		if (status == CDK_GRAB_SUCCESS)
		{
			break;
		}
		else if (i == (int) (retries / 2))
		{
			/* try nuking focus in the middle */
			gs_grab_nuke_focus (display);
		}

		/* else, wait a second and try to grab again */
		g_usleep (G_USEC_PER_SEC);
	}

	if (status != CDK_GRAB_SUCCESS)
	{
		gs_debug ("Couldn't grab devices!  (%s)",
		          grab_string (status));

		/* do not blank without a devices grab */
		return FALSE;
	}

	/* grab is good, go ahead and blank  */
	return TRUE;
}

/* this is used to grab devices to the root */
gboolean
gs_grab_grab_root (GSGrab  *grab,
                   gboolean no_pointer_grab,
                   gboolean hide_cursor)
{
	CdkDisplay *display;
	CdkWindow  *root;
	CdkScreen  *screen;
	CdkDevice  *device;
	gboolean    res;

	gs_debug ("Grabbing the root window");

	display = cdk_display_get_default ();
	device = cdk_seat_get_pointer (cdk_display_get_default_seat (display));
	cdk_device_get_position (device, &screen, NULL, NULL);
	root = cdk_screen_get_root_window (screen);

	res = gs_grab_grab_window (grab, root, display,
	                           no_pointer_grab, hide_cursor);

	return res;
}

/* this is used to grab devices to an offscreen window */
gboolean
gs_grab_grab_offscreen (GSGrab *grab,
                        gboolean no_pointer_grab,
                        gboolean hide_cursor)
{
	CdkWindow *window;
	CdkDisplay *display;
	CdkScreen  *screen;
	gboolean    res;

	gs_debug ("Grabbing an offscreen window");

	window = ctk_widget_get_window (CTK_WIDGET (grab->priv->invisible));
	screen = ctk_invisible_get_screen (CTK_INVISIBLE (grab->priv->invisible));
	display = cdk_screen_get_display (screen);
	res = gs_grab_grab_window (grab, window, display,
	                           no_pointer_grab, hide_cursor);

	return res;
}

/* this is similar to gs_grab_grab_window but doesn't fail */
void
gs_grab_move_to_window (GSGrab     *grab,
                        CdkWindow  *window,
                        CdkDisplay *display,
                        gboolean    no_pointer_grab,
                        gboolean    hide_cursor)
{
	gboolean result = FALSE;

	g_return_if_fail (GS_IS_GRAB (grab));

	xorg_lock_smasher_set_active (grab, FALSE);

	while (!result)
	{
		result = gs_grab_move (grab, window, display,
		                       no_pointer_grab, hide_cursor);
		cdk_display_flush (display);
	}
}

static void
gs_grab_class_init (GSGrabClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gs_grab_finalize;
}

static void
gs_grab_init (GSGrab *grab)
{
	grab->priv = gs_grab_get_instance_private (grab);

	grab->priv->no_pointer_grab = FALSE;
	grab->priv->hide_cursor = FALSE;
	grab->priv->invisible = ctk_invisible_new ();
	ctk_widget_show (grab->priv->invisible);
}

static void
gs_grab_finalize (GObject *object)
{
	GSGrab *grab;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GS_IS_GRAB (object));

	grab = GS_GRAB (object);

	g_return_if_fail (grab->priv != NULL);

	ctk_widget_destroy (grab->priv->invisible);

	G_OBJECT_CLASS (gs_grab_parent_class)->finalize (object);
}

GSGrab *
gs_grab_new (void)
{
	if (grab_object)
	{
		g_object_ref (grab_object);
	}
	else
	{
		grab_object = g_object_new (GS_TYPE_GRAB, NULL);
		g_object_add_weak_pointer (grab_object,
		                           (gpointer *) &grab_object);
	}

	return GS_GRAB (grab_object);
}
