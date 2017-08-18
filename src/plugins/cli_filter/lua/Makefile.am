# Makefile for cli_filter/lua plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common \
	      -DDEFAULT_SCRIPT_DIR=\"$(sysconfdir)\" \
	      -D_DEBUG_LUA

if HAVE_LUA
pkglib_LTLIBRARIES = cli_filter_lua.la

# Job submit defaults plugin.
cli_filter_lua_la_SOURCES = cli_filter_lua.c
cli_filter_lua_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS) $(lua_LIBS)
 
else
EXTRA_cli_filter_lua_la_SOURCES = cli_filter_lua.c
endif
