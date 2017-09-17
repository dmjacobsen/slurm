# Makefile for cli_filter/lua plugin

AUTOMAKE_OPTIONS = foreign
CLEANFILES = core.* cli_filter_lua_gen.c

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common \
	      -DDEFAULT_SCRIPT_DIR=\"$(sysconfdir)\" $(lua_CFLAGS)

CLI_OPT_HEADERFILES = $(srcdir)/../../../sbatch/sbatch_opt.h \
		$(srcdir)/../../../salloc/salloc_opt.h \
		$(srcdir)/../../../srun/libsrun/srun_opt.h \
		$(srcdir)/../../../bcast/file_bcast.h

CLI_OPT_STRUCTS = sbatch_options salloc_options srun_options bcast_parameters

BUILT_SOURCES = cli_filter_lua_gen.c

if HAVE_LUA
pkglib_LTLIBRARIES = cli_filter_lua.la

# Job submit defaults plugin.
cli_filter_lua_la_SOURCES = cli_filter_lua.c \
			    cli_filter_lua_gen.c

cli_filter_lua_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS) $(lua_LIBS)
cli_filter_lua_la_LIBADD = ../common/libcli_filter_common.la

cli_filter_lua_gen.c: Makefile
	/bin/sh -c "CPPFLAGS=\"-I. -I$(srcdir) -I$(top_srcdir) -I$(top_builddir) -I$(top_builddir)/slurm\" python $(srcdir)/../common/gen_cli_filter_common.py $(CLI_OPT_HEADERFILES) $(CLI_OPT_STRUCTS) --lua > cli_filter_lua_gen.c"

else
EXTRA_cli_filter_lua_la_SOURCES = cli_filter_lua.c
endif

