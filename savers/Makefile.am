## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

AM_CPPFLAGS =							\
	-I.							\
	-I$(srcdir)						\
	$(CAFE_SCREENSAVER_SAVER_CFLAGS)			\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"			\
	-DDATADIR=\""$(datadir)"\"				\
	$(WARN_CFLAGS)						\
	$(NULL)

DESKTOP_IN_IN_FILES = 			 \
	space-slide.desktop.in.in 	 \
	foot-floaters.desktop.in.in	 \
	gnome-floaters.desktop.in.in	 \
	personal-slide.desktop.in.in \
	squares.desktop.in.in	 \
	$(NULL)

space-slide.desktop.in: space-slide.desktop.in.in Makefile
	@$(edit) $< >$@
foot-floaters.desktop.in: foot-floaters.desktop.in.in Makefile
	@$(edit) $< >$@
gnome-floaters.desktop.in: gnome-floaters.desktop.in.in Makefile
	@$(edit) $< >$@
personal-slide.desktop.in: personal-slide.desktop.in.in Makefile
	@$(edit) $< >$@
squares.desktop.in: squares.desktop.in.in Makefile
	@$(edit) $< >$@

edit = sed \
	-e 's|@SLIDESHOW_SPACE_DIR[@]|$(SLIDESHOW_SPACE_DIR)|g' \
	-e 's|@FLOATERS_FOOT_LOGO_IMAGE[@]|$(FLOATERS_FOOT_LOGO_IMAGE)|g' \
	-e 's|@FLOATERS_FOOT_GNOME_LOGO_IMAGE[@]|$(FLOATERS_FOOT_GNOME_LOGO_IMAGE)|g' \
	-e 's|@SAVERDIR[@]|$(privlibexecdir)|g'

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

themesdir = $(datadir)/applications/screensavers
themes_in_files = 			\
	space-slide.desktop.in	\
	foot-floaters.desktop.in    \
	gnome-floaters.desktop.in    \
	personal-slide.desktop.in	\
	squares.desktop.in		\
	$(NULL)
themes_DATA = $(themes_in_files:.desktop.in=.desktop)

BUILT_SOURCES = 			\
	gs-theme-engine-marshal.c	\
	gs-theme-engine-marshal.h	\
	$(NULL)

gs-theme-engine-marshal.c: gs-theme-engine-marshal.list
	echo "#include \"gs-theme-engine-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gs_theme_engine_marshal --body >> $@

gs-theme-engine-marshal.h: gs-theme-engine-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gs_theme_engine_marshal --header > $@

noinst_LIBRARIES =			\
	libgs-theme-engine.a

libgs_theme_engine_a_CPPFLAGS =					\
	$(CAFE_SCREENSAVER_SAVER_CFLAGS)			\
	-DDATADIR=\""$(datadir)"\"				\
	$(NULL)

libgs_theme_engine_a_SOURCES =		\
	$(BUILT_SOURCES)		\
	gs-theme-window.h		\
	gs-theme-window.c		\
	gs-theme-engine.c		\
	gs-theme-engine.h		\
	$(NULL)

saverdir = $(libexecdir)/cafe-screensaver
saver_PROGRAMS = 	\
	floaters	\
	squares	\
	slide	\
	$(NULL)

floaters_SOURCES =	\
	floaters.c	\
	$(NULL)

floaters_LDADD =		       \
	libgs-theme-engine.a		\
	$(CAFE_SCREENSAVER_SAVER_LIBS) \
	-lm                             \
	$(NULL)

squares_SOURCES = 		\
	gste-squares.c	\
	gste-squares.h	\
	squares.c		\
	$(NULL)

squares_LDADD =			\
	libgs-theme-engine.a 		\
	$(CAFE_SCREENSAVER_SAVER_LIBS)	\
	$(NULL)

slide_SOURCES =   \
	gste-slide.c	\
	gste-slide.h	\
	xdg-user-dir-lookup.c	\
	xdg-user-dir-lookup.h	\
	slide.c		\
	$(NULL)

slide_LDADD =     \
	libgs-theme-engine.a 		\
	$(CAFE_SCREENSAVER_SAVER_LIBS)	\
	$(NULL)

EXTRA_DIST =				\
	gs-theme-engine-marshal.list	\
	$(DESKTOP_IN_IN_FILES)		\
	$(themes_in_files)		\
	$(NULL)

CLEANFILES = 				\
	$(BUILT_SOURCES)		\
	space-slide.desktop.in	\
	foot-floaters.desktop.in	\
	gnome-floaters.desktop.in	\
	squares.desktop.in		\
	personal-slide.desktop.in	\
	$(NULL)

DISTCLEANFILES = 			\
	$(themes_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in			\
	space-slide.desktop	\
	foot-floaters.desktop	\
	gnome-floaters.desktop	\
	personal-slide.desktop	\
	squares.desktop		\
	$(NULL)
