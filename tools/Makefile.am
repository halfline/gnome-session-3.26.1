bin_PROGRAMS = gnome-session-quit gnome-session-inhibit
libexec_PROGRAMS = \
	gnome-session-check-accelerated \
	gnome-session-check-accelerated-gl-helper \
	gnome-session-check-accelerated-gles-helper

if BUILD_SESSION_SELECTOR
bin_PROGRAMS += gnome-session-selector
dist_bin_SCRIPTS = gnome-session-custom-session
endif

AM_CPPFLAGS =

AM_CFLAGS = $(WARN_CFLAGS)

gnome_session_quit_SOURCES =			\
	gnome-session-quit.c

gnome_session_quit_CPPFLAGS =			\
	$(AM_CPPFLAGS)				\
	$(GNOME_SESSION_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)			\
	$(GCONF_FLAGS)				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

gnome_session_quit_LDADD =			\
	$(SM_LIBS)				\
	$(ICE_LIBS)				\
	$(GNOME_SESSION_LIBS)			\
	$(DBUS_GLIB_LIBS)			\
	$(GCONF_LIBS)

gnome_session_inhibit_SOURCES =			\
	gnome-session-inhibit.c

gnome_session_inhibit_CPPFLAGS =		\
	-I$(top_srcdir)/			\
	$(AM_CPPFLAGS)				\
	$(GNOME_SESSION_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

gnome_session_inhibit_LDADD =			\
	$(GNOME_SESSION_LIBS)

gnome_session_check_accelerated_gles_helper_SOURCES = 		\
	gnome-session-check-accelerated-common.h		\
	gnome-session-check-accelerated-gles-helper.c

gnome_session_check_accelerated_gles_helper_CPPFLAGS =	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(GLES_TEST_CFLAGS)				\
	$(GTK3_CFLAGS)

gnome_session_check_accelerated_gles_helper_LDADD =	\
	$(GLES_TEST_LIBS)				\
	$(GTK3_LIBS)					\
	$(X11_LIBS)

gnome_session_check_accelerated_gl_helper_SOURCES = 		\
	gnome-session-check-accelerated-common.h		\
	gnome-session-check-accelerated-gl-helper.c

gnome_session_check_accelerated_gl_helper_CPPFLAGS =	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(GL_TEST_CFLAGS)

gnome_session_check_accelerated_gl_helper_LDADD = 		\
	$(GL_TEST_LIBS)				\
	$(X11_LIBS)

gnome_session_check_accelerated_SOURCES =       	\
	gnome-session-check-accelerated-common.h	\
	gnome-session-check-accelerated.c

gnome_session_check_accelerated_CPPFLAGS =      \
	-DLIBEXECDIR=\""$(libexecdir)"\"	\
	$(AM_CPPFLAGS)			        \
	$(GTK3_CFLAGS)				\
	$(GL_TEST_CFLAGS)

gnome_session_check_accelerated_LDADD =         \
	$(GTK3_LIBS)				\
	$(X11_LIBS)				\
	$(GL_TEST_LIBS)

if BUILD_SESSION_SELECTOR
gnome_session_selector_CPPFLAGS =		\
	$(AM_CPPFLAGS)				\
	$(GNOME_SESSION_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)			\
	-DGTKBUILDER_DIR=\""$(pkgdatadir)"\"	\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

gnome_session_selector_LDADD = 			\
	$(DBUS_GLIB_CFLAGS)			\
	$(SESSION_SELECTOR_LIBS)

gnome_session_selector_SOURCES = 		\
	gnome-session-selector.c
endif

-include $(top_srcdir)/git.mk
