## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

saverdir = $(libexecdir)/cafe-screensaver
themesdir = $(pkgdatadir)/themes

AM_CPPFLAGS =							\
	-I.							\
	-I$(srcdir)						\
	-DCAFEMENU_I_KNOW_THIS_IS_UNSTABLE			\
	$(CAFE_SCREENSAVER_CFLAGS)				\
	$(CAFE_SCREENSAVER_DIALOG_CFLAGS)			\
	$(CAFE_SCREENSAVER_CAPPLET_CFLAGS)			\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	-DPREFIX=\""$(prefix)"\"				\
	-DBINDIR=\""$(bindir)"\"				\
	-DLIBDIR=\""$(libdir)"\"				\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"			\
	-DSAVERDIR=\""$(saverdir)"\"				\
	-DTHEMESDIR=\""$(themesdir)"\"				\
	-DGTKBUILDERDIR=\"$(pkgdatadir)\"			\
	-DPAM_SERVICE_NAME=\""cafe-screensaver"\"		\
	$(WARN_CFLAGS)						\
	$(AUTH_CFLAGS)						\
	$(DEBUG_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(LIBCAFEKBDUI_CFLAGS)					\
	$(LIBNOTIFY_CFLAGS)					\
	$(SYSTEMD_CFLAGS)					\
	$(LIBELOGIND_CFLAGS)					\
	$(NULL)

bin_PROGRAMS = \
	cafe-screensaver		\
	cafe-screensaver-command	\
	cafe-screensaver-preferences	\
	$(NULL)

libexec_PROGRAMS = \
	cafe-screensaver-dialog	\
	cafe-screensaver-gl-helper	\
	$(NULL)

noinst_PROGRAMS = 	\
	test-fade	\
	test-passwd	\
	test-watcher	\
	test-window	\
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cafe-screensaver.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

cafe_screensaver_command_SOURCES = 	\
	cafe-screensaver-command.c	\
	$(NULL)

cafe_screensaver_command_LDADD =	\
	$(CAFE_SCREENSAVER_COMMAND_LIBS)	\
	$(NULL)

AUTH_SOURCES = 			\
	gs-auth.h		\
	gs-auth-@AUTH_SCHEME@.c	\
	$(NULL)

test_fade_SOURCES = 			\
	test-fade.c 			\
	gs-fade.c	 		\
	gs-fade.h	 		\
	gs-debug.c			\
	gs-debug.h			\
	$(NULL)

test_fade_LDADD =			\
	$(CAFE_SCREENSAVER_LIBS)	\
	$(SAVER_LIBS)			\
	$(NULL)

test_passwd_SOURCES = 			\
	test-passwd.c 			\
	$(AUTH_SOURCES)			\
	setuid.c			\
	setuid.h			\
	subprocs.c			\
	subprocs.h			\
	$(NULL)

test_passwd_LDADD =			\
	$(CAFE_SCREENSAVER_DIALOG_LIBS)\
	$(AUTH_LIBS)			\
	$(NULL)

test_watcher_SOURCES =			\
	test-watcher.c			\
	gs-watcher.h			\
	gs-watcher-x11.c		\
	gs-marshal.c 			\
	gs-marshal.h			\
	gs-debug.c			\
	gs-debug.h			\
	$(NULL)

test_watcher_LDADD =			\
	$(CAFE_SCREENSAVER_LIBS)	\
	$(SAVER_LIBS)			\
	$(NULL)

test_window_SOURCES =			\
	test-window.c			\
	gs-window.h			\
	gs-window-x11.c			\
	gs-grab-x11.c			\
	gs-grab.h			\
	gs-marshal.c 			\
	gs-marshal.h			\
	gs-debug.c			\
	gs-debug.h			\
	subprocs.c			\
	subprocs.h			\
	$(NULL)

test_window_LDADD =			\
	$(CAFE_SCREENSAVER_LIBS)	\
	$(SAVER_LIBS)			\
	$(NULL)

cafe_screensaver_dialog_SOURCES = 	\
	cafe-screensaver-dialog.c	\
	gs-lock-plug.c			\
	gs-lock-plug.h			\
	gs-debug.c			\
	gs-debug.h			\
	setuid.c			\
	setuid.h			\
	subprocs.c			\
	subprocs.h			\
	$(AUTH_SOURCES)			\
	$(NULL)

EXTRA_cafe_screensaver_dialog_SOURCES = \
	gs-auth-pam.c		\
	gs-auth-helper.c	\
	gs-auth-pwent.c		\
	gs-auth-bsdauth.c	\
	$(NULL)

cafe_screensaver_dialog_LDADD =	\
	$(CAFE_SCREENSAVER_DIALOG_LIBS)\
	$(SAVER_LIBS)			\
	$(AUTH_LIBS)			\
	$(LIBCAFEKBDUI_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(NULL)

BUILT_SOURCES = 		\
	gs-marshal.c 		\
	gs-marshal.h		\
	$(NULL)

gs-marshal.c: gs-marshal.list
	echo "#include \"gs-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gs_marshal --body >> $@

gs-marshal.h: gs-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gs_marshal --header > $@

cafe_screensaver_SOURCES =	\
	cafe-screensaver.c	\
	cafe-screensaver.h	\
	gs-monitor.c		\
	gs-monitor.h		\
	gs-watcher-x11.c	\
	gs-watcher.h		\
	gs-listener-dbus.c	\
	gs-listener-dbus.h	\
	gs-manager.c		\
	gs-manager.h		\
	gs-window-x11.c		\
	gs-window.h		\
	gs-prefs.c		\
	gs-prefs.h		\
	gs-theme-manager.c	\
	gs-theme-manager.h	\
	gs-job.c		\
	gs-job.h		\
	gs-debug.c		\
	gs-debug.h		\
	subprocs.c		\
	subprocs.h		\
	gs-grab-x11.c		\
	gs-grab.h		\
	gs-fade.c		\
	gs-fade.h		\
	$(BUILT_SOURCES)	\
	$(NULL)

cafe_screensaver_gl_helper_LDADD =	\
	$(CAFE_SCREENSAVER_LIBS)	\
	$(GL_LIBS)			\
	$(NULL)

cafe_screensaver_gl_helper_SOURCES =	\
	cafe-screensaver-gl-helper.c	\
	gs-visual-gl.c			\
	gs-visual-gl.h			\
	$(NULL)

cafe_screensaver_LDADD =		\
	$(CAFE_SCREENSAVER_LIBS)	\
	$(SAVER_LIBS)			\
	$(SYSTEMD_LIBS)			\
	$(LIBELOGIND_LIBS)		\
	$(NULL)

cafe_screensaver_LDFLAGS = -export-dynamic

cafe-screensaver-preferences-resources.h cafe-screensaver-preferences-resources.c: $(srcdir)/../data/org.cafe.screensaver.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir)/../data $(srcdir)/../data/org.cafe.screensaver.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir)/../data --generate --c-name screensaver $<

nodist_cafe_screensaver_preferences_SOURCES = \
	cafe-screensaver-preferences-resources.h \
	cafe-screensaver-preferences-resources.c \
	$(NULL)
cafe_screensaver_preferences_SOURCES =	\
	cafe-screensaver-preferences.c	\
	copy-theme-dialog.c		\
	copy-theme-dialog.h		\
	gs-theme-manager.c		\
	gs-theme-manager.h		\
	gs-job.c			\
	gs-job.h			\
	gs-debug.c			\
	gs-debug.h			\
	subprocs.c			\
	subprocs.h			\
	$(NULL)

cafe_screensaver_preferences_LDADD =	\
	$(CAFE_SCREENSAVER_CAPPLET_LIBS)	\
	$(NULL)

EXTRA_DIST =				\
	debug-screensaver.sh		\
	gs-marshal.list			\
	cafe-screensaver.desktop.in	\
	$(NULL)

CLEANFILES = 				\
	$(desktop_DATA) 		\
	cafe-screensaver.desktop.in	\
	cafe-screensaver-preferences-resources.h \
	cafe-screensaver-preferences-resources.c \
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in

install-exec-hook:
	@if [ "x@NEED_SETUID@" = "xyes" ]; then \
	    echo "***" ; \
	    echo "***  Warning: cafe-screensaver has been compiled with support for" ; \
	    if [ "x@AUTH_SCHEME@" = "xbsdauth" ]; then \
		echo "***           bsd_auth(3) and must be installed as a setuid root" ; \
	    else \
	        echo "***           shadow passwords. If your system actually uses shadow" ; \
	        echo "***           passwords then it must be installed as a setuid root" ; \
	    fi; \
	    echo "***           program in order for locking to work. To do this, you" ; \
	    echo "***           must run:" ; \
	    echo "***" ; \
	    echo "***           chown root $(DESTDIR)$(libexecdir)/cafe-screensaver-dialog" ; \
	    echo "***           chmod +s $(DESTDIR)$(libexecdir)/cafe-screensaver-dialog" ; \
	    echo "***" ; \
	    echo "***           For now, it will be installed non-setuid, which" ; \
	    echo "***           means that locking might not work." ; \
	    echo "***" ; \
	fi
