/*****************************************************************************\
 *  cli_filter_lua.c - lua CLI option processing specifications.
 *****************************************************************************
 *  Copyright (C) 2017 Regents of the University of California
 *  Produced at Lawrence Berkeley National Laboratory
 *  Written by Douglas Jacobsen <dmjacobsen@lbl.gov>
 *  All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"
#include "src/common/slurm_xlator.h"
#include "src/common/cli_filter.h"
#include "src/salloc/salloc_opt.h"
#include "src/sbatch/sbatch_opt.h"
#include "src/srun/libsrun/srun_opt.h"
#include "src/common/xlua.h"

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  SLURM uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "auth" for SLURM authentication) and <method> is a
 * description of how this plugin satisfies that application.  SLURM will
 * only load authentication plugins if the plugin_type string has a prefix
 * of "auth/".
 *
 * plugin_version - an unsigned 32-bit integer containing the Slurm version
 * (major.minor.micro combined into a single number).
 */
const char plugin_name[]       	= "cli filter defaults plugin";
const char plugin_type[]       	= "cli_filter/lua";
const uint32_t plugin_version   = SLURM_VERSION_NUMBER;
static const char lua_script_path[] = DEFAULT_SCRIPT_DIR "/cli_filter.lua";
static lua_State *L = NULL;
static char *user_msg = NULL;

static int _log_lua_msg(lua_State *L);
static int _log_lua_error(lua_State *L);
static int _log_lua_user_msg(lua_State *L);
static int _load_script(void);
static void _stack_dump (char *header, lua_State *L);

static const struct luaL_Reg slurm_functions [] = {
	{ "log",	_log_lua_msg   },
	{ "error",	_log_lua_error },
	{ "user_msg",	_log_lua_user_msg },
	{ NULL,		NULL        }
};

struct option_string;
static bool _lua_pushstring(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushbool(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushint(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushint32_t(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushuid(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushgid(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);
static bool _lua_pushstringarray(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L);



struct option_string {
	char *name;
	size_t offset;
	bool (*func)(void *, const char *, const struct option_string *,
		    lua_State *);
};

static struct option_string salloc_opt_names[] = {
	{ "progname", offsetof(salloc_opt_t, progname), _lua_pushstring },
	{ "user",     offsetof(salloc_opt_t, user), _lua_pushstring },
	{ "uid",      offsetof(salloc_opt_t, uid), _lua_pushuid },
	{ "gid",      offsetof(salloc_opt_t, gid), _lua_pushgid },
	{ "euid",     offsetof(salloc_opt_t, euid), _lua_pushuid },
	{ "egid",     offsetof(salloc_opt_t, egid), _lua_pushgid },
	{ "cwd",      offsetof(salloc_opt_t, cwd), _lua_pushstring },
	{ "ntasks",   offsetof(salloc_opt_t, ntasks), _lua_pushint },
	{ "ntasks_set", offsetof(salloc_opt_t, ntasks_set), _lua_pushbool },
	{ "cpus_per_task", offsetof(salloc_opt_t, cpus_per_task), _lua_pushint },
	{ "cpus_per_task_set", offsetof(salloc_opt_t, cpus_set), _lua_pushbool },
	{ "min_nodes", offsetof(salloc_opt_t, min_nodes), _lua_pushint },
	{ "max_nodes", offsetof(salloc_opt_t, max_nodes), _lua_pushint },
	{ "nodes_set", offsetof(salloc_opt_t, nodes_set), _lua_pushbool },
	{ NULL, 0, NULL }
};

static struct option_string sbatch_opt_names[] = {
	{ "clusters", offsetof(sbatch_opt_t, clusters), _lua_pushstring },
	{ "progname", offsetof(sbatch_opt_t, progname), _lua_pushstring },
	{ "argc", offsetof(sbatch_opt_t, script_argc), _lua_pushint },
	{ "argv", offsetof(sbatch_opt_t, script_argv), _lua_pushstringarray },
	{ "user",     offsetof(sbatch_opt_t, user), _lua_pushstring },
	{ "uid",      offsetof(sbatch_opt_t, uid), _lua_pushuid },
	{ "gid",      offsetof(sbatch_opt_t, gid), _lua_pushgid },
	{ "euid",     offsetof(sbatch_opt_t, euid), _lua_pushuid },
	{ "egid",     offsetof(sbatch_opt_t, egid), _lua_pushgid },
	{ "cwd",   offsetof(sbatch_opt_t, cwd), _lua_pushstring },
	{ "ntasks",   offsetof(sbatch_opt_t, ntasks), _lua_pushint },
	{ "ntasks_set", offsetof(sbatch_opt_t, ntasks_set), _lua_pushbool },
	{ "cpus_per_task", offsetof(sbatch_opt_t, cpus_per_task), _lua_pushint },
	{ "cpus_per_task_set", offsetof(sbatch_opt_t, cpus_set), _lua_pushbool },
	{ "min_nodes", offsetof(sbatch_opt_t, min_nodes), _lua_pushint },
	{ "max_nodes", offsetof(sbatch_opt_t, max_nodes), _lua_pushint },
	{ "nodes_set", offsetof(sbatch_opt_t, nodes_set), _lua_pushbool },
	{ NULL, 0, NULL }
};

static struct option_string srun_opt_names[] = {
	{ "progname", offsetof(srun_opt_t, progname), _lua_pushstring },
	{ "multi_prog", offsetof(srun_opt_t, multi_prog), _lua_pushbool },
	{ "multi_prog_cmds", offsetof(srun_opt_t, multi_prog_cmds), _lua_pushint32_t },
	{ "argc", offsetof(srun_opt_t, argc), _lua_pushint },
	{ "argv", offsetof(srun_opt_t, argv), _lua_pushstringarray },
	{ "user",     offsetof(srun_opt_t, user), _lua_pushstring },
	{ "uid",      offsetof(srun_opt_t, uid), _lua_pushuid },
	{ "gid",      offsetof(srun_opt_t, gid), _lua_pushgid },
	{ "euid",     offsetof(srun_opt_t, euid), _lua_pushuid },
	{ "egid",     offsetof(srun_opt_t, egid), _lua_pushgid },
	{ "cwd",   offsetof(srun_opt_t, cwd), _lua_pushstring },
	{ "cwd_set",   offsetof(srun_opt_t, cwd_set), _lua_pushbool },
	{ "ntasks",   offsetof(srun_opt_t, ntasks), _lua_pushint },
	{ "ntasks_set", offsetof(srun_opt_t, ntasks_set), _lua_pushbool },
	{ "cpus_per_task", offsetof(srun_opt_t, cpus_per_task), _lua_pushint },
	{ "cpus_per_task_set", offsetof(srun_opt_t, cpus_set), _lua_pushbool },
	{ "max_threads", offsetof(srun_opt_t, max_threads), _lua_pushint32_t },
	{ "min_nodes", offsetof(srun_opt_t, min_nodes), _lua_pushint },
	{ "max_nodes", offsetof(srun_opt_t, max_nodes), _lua_pushint },
	{ "nodes_set", offsetof(srun_opt_t, nodes_set), _lua_pushbool },
	{ NULL, 0, NULL }
};

/*
 *  NOTE: The init callback should never be called multiple times,
 *   let alone called from multiple threads. Therefore, locking
 *   is unnecessary here.
 */
int init(void)
{
        int rc = SLURM_SUCCESS;

        /*
         * Need to dlopen() the Lua library to ensure plugins see
         * appropriate symptoms
         */
        if ((rc = xlua_dlopen()) != SLURM_SUCCESS)
                return rc;

        return _load_script();
}

int fini(void)
{
        lua_close (L);
        return SLURM_SUCCESS;
}


/* in the future this function and the option_string data structures should
 * probably be converted to use a binary search or other faster method for
 * discovering the requested option */
const struct option_string *find_opt_str(const char *name,
                                         const struct option_string *opt_str)
{
	size_t idx = 0;
	for (idx = 0; opt_str[idx].name; idx++)
		if (!xstrcmp(opt_str[idx].name, name)) {
			return &(opt_str[idx]);
		}
	return NULL;
}

static bool _lua_pushstring(void *data, const char *name,
			    const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;

	char **tgt = (char **) (data + opt_str->offset);
	lua_pushstring(L, *tgt);
	return true;
}

static bool _lua_pushbool(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	bool *tgt = (bool *) (data + opt_str->offset);
	lua_pushboolean(L, (int) *tgt);
	return true;
}

static bool _lua_pushint(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	int *tgt = (int *) (data + opt_str->offset);
	lua_pushnumber(L, (double) *tgt);
	return true;
}

static bool _lua_pushint32_t(void *data, const char *name,
			 const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	int32_t *tgt = (int *) (data + opt_str->offset);
	lua_pushnumber(L, (double) *tgt);
	return true;
}

static bool _lua_pushuid(void *data, const char *name,
			   const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	uid_t *tgt = (uid_t *) (data + opt_str->offset);
	lua_pushnumber(L, (double) *tgt);
	return true;
}

static bool _lua_pushgid(void *data, const char *name,
			   const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	gid_t *tgt = (gid_t *) (data + opt_str->offset);
	lua_pushnumber(L, *tgt);
	return true;
}

static int _stringarray_field_index(lua_State *L)
{
	char **strarray = NULL;
	int strarray_sz = 0;
	int idx = luaL_checkint(L, 2);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_stringarray");
	strarray = lua_touserdata(L, -1);
	lua_getfield(L, -2, "_stringarray_sz");
	strarray_sz = luaL_checkint(L, -1);

	/* lua indexing starts at 1 */
	if (idx > 0 && idx <= strarray_sz) {
		lua_pushstring(L, strarray[idx - 1]);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static bool _lua_pushstringarray(void *data, const char *name,
			   const struct option_string *opt_str, lua_State *L)
{
	if (!data || !name || !opt_str || !L)
		return false;
	char ***tgt = (char ***) (data + opt_str->offset);
	char **ptr = *tgt;
	int sz = 0;
	for ( ptr = *tgt; ptr && *ptr; ptr++) {
		sz++;
	}

	lua_newtable(L);
	lua_newtable(L);
	lua_pushcfunction(L, _stringarray_field_index);
	lua_setfield(L, -2, "__index");
/* TODO allow plugin to extend string array
	lua_pushcfunction(L, _stringarray_field);
	lua_setfield(L, -2, "__newindex");
*/
	lua_pushlightuserdata(L, *tgt);
	lua_setfield(L, -2, "_stringarray");
	lua_pushnumber(L, (double) sz);
	lua_setfield(L, -2, "_stringarray_sz");
	lua_setmetatable(L, -2);

	return true;
}

static int _get_option_field_index(lua_State *L)
{
	const char *name;
	const struct option_string *opt_str;
	const struct option_string *req_option = NULL;
	void *data = NULL;

	name = luaL_checkstring(L, 2);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_opt_str");
	opt_str = lua_touserdata(L, -1);
	lua_getfield(L, -2, "_opt_data");
	data = lua_touserdata(L, -1);

	req_option = find_opt_str(name, opt_str);
	if ((req_option->func)(data, name, req_option, L))
		return 1;

	return 0;
}

static int _set_option_field(lua_State *L)
{
	const char *name = NULL;
	const struct option_string *opt_str;
	const struct option_string *req_option = NULL;
	void *data = NULL;

	name = luaL_checkstring(L, 2);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_opt_str");
	opt_str = lua_touserdata(L, -1);
	lua_getfield(L, -1, "_opt_data");
	data = lua_touserdata(L, -1);
	
	return 0;
}

static void _push_options(int cli_type, void *opt_data)
{
	struct option_string *opt_str = NULL;
	if (cli_type == CLI_SALLOC) {
		opt_str = salloc_opt_names;
	} else if (cli_type == CLI_SBATCH) {
		opt_str = sbatch_opt_names;
		sbatch_opt_t *sb = (sbatch_opt_t *) opt_data;
	
	} else if (cli_type == CLI_SRUN) {
		opt_str = srun_opt_names;
	}

	lua_newtable(L);

	lua_newtable(L);
	lua_pushcfunction(L, _get_option_field_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, _set_option_field);
	lua_setfield(L, -2, "__newindex");
	/* Store the job descriptor in the metatable, so the index
	 * function knows which struct it's getting data for.
	 */
	lua_pushlightuserdata(L, opt_str);
	lua_setfield(L, -2, "_opt_str");
	lua_pushlightuserdata(L, opt_data);
	lua_setfield(L, -2, "_opt_data");
	lua_setmetatable(L, -2);
}

static int _log_lua_error (lua_State *L)
{
	const char *prefix  = "cli_filter/lua";
	const char *msg     = lua_tostring (L, -1);
	error ("%s: %s", prefix, msg);
	return (0);
}

static int _log_lua_user_msg (lua_State *L)
{
	const char *msg = lua_tostring(L, -1);

	xfree(user_msg);
	user_msg = xstrdup(msg);
	return (0);
}


/*
 *  Lua interface to SLURM log facility:
 */
static int _log_lua_msg (lua_State *L)
{
	const char *prefix  = "cli_filter/lua";
	int        level    = 0;
	const char *msg;

	/*
	 *  Optional numeric prefix indicating the log level
	 *  of the message.
	 */

	/*
	 *  Pop message off the lua stack
	 */
	msg = lua_tostring(L, -1);
	lua_pop (L, 1);
	/*
	 *  Pop level off stack:
	 */
	level = (int)lua_tonumber (L, -1);
	lua_pop (L, 1);

	/*
	 *  Call appropriate slurm log function based on log-level argument
	 */
	if (level > 4)
		debug4 ("%s: %s", prefix, msg);
	else if (level == 4)
		debug3 ("%s: %s", prefix, msg);
	else if (level == 3)
		debug2 ("%s: %s", prefix, msg);
	else if (level == 2)
		debug ("%s: %s", prefix, msg);
	else if (level == 1)
		verbose ("%s: %s", prefix, msg);
	else if (level == 0)
		info ("%s: %s", prefix, msg);
	return (0);
}


/*
 *  check that global symbol [name] in lua script is a function
 */
static int _check_lua_script_function(const char *name)
{
	int rc = 0;
	lua_getglobal(L, name);
	if (!lua_isfunction(L, -1))
		rc = -1;
	lua_pop(L, -1);
	return (rc);
}

/*
 *   Verify all required functions are defined in the job_submit/lua script
 */
static int _check_lua_script_functions(void)
{
	int rc = 0;
	int i;
	const char *fns[] = {
		"slurm_cli_pre_submit",
		NULL
	};

	i = 0;
	do {
		if (_check_lua_script_function(fns[i]) < 0) {
			error("cli_filter/lua: %s: "
			      "missing required function %s",
			      lua_script_path, fns[i]);
			rc = -1;
		}
	} while (fns[++i]);

	return (rc);
}

static void _lua_table_register(lua_State *L, const char *libname,
				const luaL_Reg *l)
{
#if LUA_VERSION_NUM == 501
	luaL_register(L, libname, l);
#else
	luaL_setfuncs(L, l, 0);
	if (libname)
		lua_setglobal(L, libname);
#endif
}

static void _register_lua_slurm_output_functions (void)
{
	/*
	 *  Register slurm output functions in a global "slurm" table
	 */
	lua_newtable (L);
	_lua_table_register(L, NULL, slurm_functions);

	/*
	 *  Create more user-friendly lua versions of SLURM log functions.
	 */
	luaL_loadstring (L, "slurm.error (string.format(unpack({...})))");
	lua_setfield (L, -2, "log_error");
	luaL_loadstring (L, "slurm.log (0, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_info");
	luaL_loadstring (L, "slurm.log (1, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_verbose");
	luaL_loadstring (L, "slurm.log (2, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_debug");
	luaL_loadstring (L, "slurm.log (3, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_debug2");
	luaL_loadstring (L, "slurm.log (4, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_debug3");
	luaL_loadstring (L, "slurm.log (5, string.format(unpack({...})))");
	lua_setfield (L, -2, "log_debug4");
	luaL_loadstring (L, "slurm.user_msg (string.format(unpack({...})))");
	lua_setfield (L, -2, "log_user");

	/*
	 * Error codes: slurm.SUCCESS, slurm.FAILURE, slurm.ERROR, etc.
	 */
	lua_pushnumber (L, SLURM_FAILURE);
	lua_setfield (L, -2, "FAILURE");
	lua_pushnumber (L, SLURM_ERROR);
	lua_setfield (L, -2, "ERROR");
	lua_pushnumber (L, SLURM_SUCCESS);
	lua_setfield (L, -2, "SUCCESS");
	lua_pushnumber (L, ESLURM_INVALID_LICENSES);
	lua_setfield (L, -2, "ESLURM_INVALID_LICENSES");

	/*
	 * Other definitions needed to interpret data
	 * slurm.MEM_PER_CPU, slurm.NO_VAL, etc.
	 */
	lua_pushnumber (L, ALLOC_SID_ADMIN_HOLD);
	lua_setfield (L, -2, "ALLOC_SID_ADMIN_HOLD");
	lua_pushnumber (L, ALLOC_SID_USER_HOLD);
	lua_setfield (L, -2, "ALLOC_SID_USER_HOLD");
	lua_pushnumber (L, INFINITE);
	lua_setfield (L, -2, "INFINITE");
	lua_pushnumber (L, INFINITE64);
	lua_setfield (L, -2, "INFINITE64");
	lua_pushnumber (L, MAIL_JOB_BEGIN);
	lua_setfield (L, -2, "MAIL_JOB_BEGIN");
	lua_pushnumber (L, MAIL_JOB_END);
	lua_setfield (L, -2, "MAIL_JOB_END");
	lua_pushnumber (L, MAIL_JOB_FAIL);
	lua_setfield (L, -2, "MAIL_JOB_FAIL");
	lua_pushnumber (L, MAIL_JOB_REQUEUE);
	lua_setfield (L, -2, "MAIL_JOB_REQUEUE");
	lua_pushnumber (L, MAIL_JOB_TIME100);
	lua_setfield (L, -2, "MAIL_JOB_TIME100");
	lua_pushnumber (L, MAIL_JOB_TIME90);
	lua_setfield (L, -2, "MAIL_JOB_TIME890");
	lua_pushnumber (L, MAIL_JOB_TIME80);
	lua_setfield (L, -2, "MAIL_JOB_TIME80");
	lua_pushnumber (L, MAIL_JOB_TIME50);
	lua_setfield (L, -2, "MAIL_JOB_TIME50");
	lua_pushnumber (L, MAIL_JOB_STAGE_OUT);
	lua_setfield (L, -2, "MAIL_JOB_STAGE_OUT");
	lua_pushnumber (L, MEM_PER_CPU);
	lua_setfield (L, -2, "MEM_PER_CPU");
	lua_pushnumber (L, NICE_OFFSET);
	lua_setfield (L, -2, "NICE_OFFSET");
	lua_pushnumber (L, JOB_SHARED_NONE);
	lua_setfield (L, -2, "JOB_SHARED_NONE");
	lua_pushnumber (L, JOB_SHARED_OK);
	lua_setfield (L, -2, "JOB_SHARED_OK");
	lua_pushnumber (L, JOB_SHARED_USER);
	lua_setfield (L, -2, "JOB_SHARED_USER");
	lua_pushnumber (L, JOB_SHARED_MCS);
	lua_setfield (L, -2, "JOB_SHARED_MCS");
	lua_pushnumber (L, NO_VAL64);
	lua_setfield (L, -2, "NO_VAL64");
	lua_pushnumber (L, NO_VAL);
	lua_setfield (L, -2, "NO_VAL");
	lua_pushnumber (L, (uint16_t) NO_VAL);
	lua_setfield (L, -2, "NO_VAL16");
	lua_pushnumber (L, (uint8_t) NO_VAL);
	lua_setfield (L, -2, "NO_VAL8");

	/*
	 * job_desc bitflags
	 */
	lua_pushnumber (L, GRES_ENFORCE_BIND);
	lua_setfield (L, -2, "GRES_ENFORCE_BIND");
	lua_pushnumber (L, KILL_INV_DEP);
	lua_setfield (L, -2, "KILL_INV_DEP");
	lua_pushnumber (L, NO_KILL_INV_DEP);
	lua_setfield (L, -2, "NO_KILL_INV_DEP");
	lua_pushnumber (L, SPREAD_JOB);
	lua_setfield (L, -2, "SPREAD_JOB");
	lua_pushnumber (L, USE_MIN_NODES);
	lua_setfield (L, -2, "USE_MIN_NODES");

	lua_setglobal (L, "slurm");
}

static void _register_lua_slurm_struct_functions (void)
{
#if 0
	lua_pushcfunction(L, _get_job_env_field_name);
	lua_setglobal(L, "_get_job_env_field_name");
	lua_pushcfunction(L, _get_job_req_field_name);
	lua_setglobal(L, "_get_job_req_field_name");
	lua_pushcfunction(L, _set_job_env_field);
	lua_setglobal(L, "_set_job_env_field");
	lua_pushcfunction(L, _set_job_req_field);
	lua_setglobal(L, "_set_job_req_field");
	lua_pushcfunction(L, _get_part_rec_field);
	lua_setglobal(L, "_get_part_rec_field");
#endif
}


static int _load_script(void)
{
	int rc = SLURM_SUCCESS;
	struct stat st;

	/*
	 * Need to dlopen() the Lua library to ensure plugins see
	 * appropriate symptoms
	 */
	if ((rc = xlua_dlopen()) != SLURM_SUCCESS)
		return rc;


	if (stat(lua_script_path, &st) != 0) {
		return error("Unable to stat %s: %s",
		             lua_script_path, strerror(errno));
	}
	
	/*
	 *  Initilize lua
	 */
	L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_loadfile(L, lua_script_path)) {
		rc = error("lua: %s: %s", lua_script_path,
		           lua_tostring(L, -1));
		lua_pop(L, 1);
		return rc;
	}

	/*
	 *  Register SLURM functions in lua state:
	 *  logging and slurm structure read/write functions
	 */
	_register_lua_slurm_output_functions();
	_register_lua_slurm_struct_functions();

	/*
	 *  Run the user script:
	 */
	if (lua_pcall(L, 0, 1, 0) != 0) {
		rc = error("cli_filter/lua: %s: %s",
		           lua_script_path, lua_tostring(L, -1));
		lua_pop(L, 1);
		return rc;
	}

	/*
	 *  Get any return code from the lua script
	 */
	rc = (int) lua_tonumber(L, -1);
	if (rc != SLURM_SUCCESS) {
		(void) error("cli_filter/lua: %s: returned %d on load",
		             lua_script_path, rc);
		lua_pop (L, 1);
		return rc;
	}

	/*
	 *  Check for required lua script functions:
	 */
	rc = _check_lua_script_functions();
	if (rc != SLURM_SUCCESS) {
		return rc;
	}

	return SLURM_SUCCESS;
}

static void _stack_dump (char *header, lua_State *L)
{
#if _DEBUG_LUA
	int i;
	int top = lua_gettop(L);

	info("%s: dumping cli_filter/lua stack, %d elements", header, top);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TSTRING:
				info("string[%d]:%s", i, lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				info("boolean[%d]:%s", i,
				     lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				info("number[%d]:%d", i,
				     (int) lua_tonumber(L, i));
				break;
			default:
				info("other[%d]:%s", i, lua_typename(L, type));
				break;
		}
	}
#endif
}


extern int pre_submit(int cli_type, void *opt, char **err_msg) {
	int rc = SLURM_ERROR;

	(void) _load_script();

	/*
	 *  All lua script functions should have been verified during
	 *   initialization:
	 */
	lua_getglobal(L, "slurm_cli_pre_submit");
	if (lua_isnil(L, -1))
		goto out;

	_push_options(cli_type, opt);

	_stack_dump("cli_filter, before lua_pcall", L);
	if (lua_pcall (L, 1, 1, 0) != 0) {
		error("%s/lua: %s: %s",
		      __func__, lua_script_path, lua_tostring (L, -1));
	} else {
		if (lua_isnumber(L, -1)) {
			rc = lua_tonumber(L, -1);
		} else {
			info("%s/lua: %s: non-numeric return code",
			      __func__, lua_script_path);
			rc = SLURM_SUCCESS;
		}
		lua_pop(L, 1);
	}
	_stack_dump("cli_filter, after lua_pcall", L);
	if (user_msg) {
		if (err_msg) {
			*err_msg = user_msg;
			user_msg = NULL;
		} else
			xfree(user_msg);
	}

out:	lua_close (L);
	return rc;
}

extern int post_submit(int cli_type, uint32_t jobid, void *opt, char **err_msg) {
	return SLURM_SUCCESS;
}

#if 0
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/common/assoc_mgr.h"
#include "src/slurmctld/locks.h"
#include "src/slurmctld/slurmctld.h"
#include "src/slurmctld/reservation.h"




/* Get the default account for a user (or NULL if not present) */
static char *_get_default_account(uint32_t user_id)
{
	slurmdb_user_rec_t user;

	memset(&user, 0, sizeof(slurmdb_user_rec_t));
	user.uid = user_id;
	if (assoc_mgr_fill_in_user(acct_db_conn, &user, accounting_enforce,
				   NULL) != SLURM_ERROR) {
		return user.default_acct;
	} else {
		return NULL;
	}
}

/* Get the default QOS for an association (or NULL if not present) */
static char *_get_default_qos(uint32_t user_id, char *account, char *partition)
{
	slurmdb_assoc_rec_t assoc;
	slurmdb_qos_rec_t qos;
	uint32_t qos_id = 0;

	memset(&assoc, 0, sizeof(slurmdb_assoc_rec_t));
	assoc.uid = user_id;
	assoc.partition = partition;
	if (account) {
		assoc.acct = account;
	} else {
		assoc.acct = _get_default_account(user_id);
	}

	if (assoc_mgr_fill_in_assoc(acct_db_conn, &assoc, accounting_enforce,
				    NULL, false) != SLURM_ERROR)
		qos_id = assoc.def_qos_id;

	if (!qos_id)
		return NULL;

	memset(&qos, 0, sizeof(slurmdb_qos_rec_t));
	qos.id = qos_id;
	if (assoc_mgr_fill_in_qos(acct_db_conn, &qos, accounting_enforce,
				  NULL, false) != SLURM_ERROR) {
		return qos.name;
	} else {
		return NULL;
	}
}

/* Get fields in an existing slurmctld job record.
 *
 * This is an incomplete list of job record fields. Add more as needed and
 * send patches to slurm-dev@schedmd.com.
 */
static int _job_rec_field(const struct job_record *job_ptr,
                          const char *name)
{
	int i;

	if (job_ptr == NULL) {
		error("_job_rec_field: job_ptr is NULL");
		lua_pushnil (L);
	} else if (!xstrcmp(name, "account")) {
		lua_pushstring (L, job_ptr->account);
	} else if (!xstrcmp(name, "admin_comment")) {
		lua_pushstring (L, job_ptr->admin_comment);
	} else if (!xstrcmp(name, "burst_buffer")) {
		lua_pushstring (L, job_ptr->burst_buffer);
	} else if (!xstrcmp(name, "comment")) {
		lua_pushstring (L, job_ptr->comment);
	} else if (!xstrcmp(name, "delay_boot")) {
		lua_pushnumber (L, job_ptr->delay_boot);
	} else if (!xstrcmp(name, "direct_set_prio")) {
		lua_pushnumber (L, job_ptr->direct_set_prio);
	} else if (!xstrcmp(name, "features")) {
		if (job_ptr->details)
			lua_pushstring (L, job_ptr->details->features);
		else
			lua_pushnil (L);
	} else if (!xstrcmp(name, "gres")) {
		lua_pushstring (L, job_ptr->gres);
	} else if (!xstrcmp(name, "job_id")) {
		lua_pushnumber (L, job_ptr->job_id);
	} else if (!xstrcmp(name, "job_state")) {
		lua_pushnumber (L, job_ptr->job_state);
	} else if (!xstrcmp(name, "licenses")) {
		lua_pushstring (L, job_ptr->licenses);
	} else if (!xstrcmp(name, "max_cpus")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->max_cpus);
		else
			lua_pushnumber (L, 0);
	} else if (!xstrcmp(name, "max_nodes")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->max_nodes);
		else
			lua_pushnumber (L, 0);
	} else if (!xstrcmp(name, "min_cpus")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->min_cpus);
		else
			lua_pushnumber (L, 0);
	} else if (!xstrcmp(name, "min_nodes")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->min_nodes);
		else
			lua_pushnumber (L, 0);
	} else if (!xstrcmp(name, "nice")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->nice);
		else
			lua_pushnumber (L, (uint16_t)NO_VAL);
	} else if (!xstrcmp(name, "partition")) {
		lua_pushstring (L, job_ptr->partition);
	} else if (!xstrcmp(name, "pn_min_cpus")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->pn_min_cpus);
		else
			lua_pushnumber (L, NO_VAL);
	} else if (!xstrcmp(name, "pn_min_memory")) {
		if (job_ptr->details)
			lua_pushnumber (L, job_ptr->details->pn_min_memory);
		else
			lua_pushnumber (L, NO_VAL64);
	} else if (!xstrcmp(name, "priority")) {
		lua_pushnumber (L, job_ptr->priority);
	} else if (!xstrcmp(name, "qos")) {
		if (job_ptr->qos_ptr) {
			slurmdb_qos_rec_t *qos_ptr =
				(slurmdb_qos_rec_t *)job_ptr->qos_ptr;
			lua_pushstring (L, qos_ptr->name);
		} else {
			lua_pushnil (L);
		}
	} else if (!xstrcmp(name, "reboot")) {
		lua_pushnumber (L, job_ptr->reboot);
	} else if (!xstrcmp(name, "req_switch")) {
		lua_pushnumber (L, job_ptr->req_switch);
	} else if (!xstrcmp(name, "spank_job_env")) {
		if ((job_ptr->spank_job_env_size == 0) ||
		    (job_ptr->spank_job_env == NULL)) {
			lua_pushnil (L);
		} else {
			lua_newtable(L);
			for (i = 0; i < job_ptr->spank_job_env_size; i++) {
				if (job_ptr->spank_job_env[i] != NULL) {
					lua_pushnumber (L, i);
					lua_pushstring (L,
						job_ptr->spank_job_env[i]);
					lua_settable (L, -3);
				}
			}
		}
	} else if (!xstrcmp(name, "spank_job_env_size")) {
		lua_pushnumber (L, job_ptr->spank_job_env_size);
	} else if (!xstrcmp(name, "time_limit")) {
		lua_pushnumber (L, job_ptr->time_limit);
	} else if (!xstrcmp(name, "time_min")) {
		lua_pushnumber (L, job_ptr->time_min);
	} else if (!xstrcmp(name, "wait4switch")) {
		lua_pushnumber (L, job_ptr->wait4switch);
	} else if (!xstrcmp(name, "wckey")) {
		lua_pushstring (L, job_ptr->wckey);
	} else {
		lua_pushnil (L);
	}

	return 1;
}

/* Get fields in an existing slurmctld job_record */
static int _job_rec_field_index(lua_State *L)
{
	const char *name = luaL_checkstring(L, 2);
	struct job_record *job_ptr;

	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_job_rec_ptr");
	job_ptr = lua_touserdata(L, -1);

	return _job_rec_field(job_ptr, name);
}

/* Set fields in the job request structure on job submit or modify */
static int _set_job_env_field(lua_State *L)
{
	const char *name, *value_str;
	struct job_descriptor *job_desc;
	char *name_eq = NULL;
	int i, j, name_len;

	name = luaL_checkstring(L, 2);
	name_eq = xstrdup(name);
	xstrcat(name_eq, "=");
	name_len = strlen(name_eq);
	lua_getmetatable(L, -3);
	lua_getfield(L, -1, "_job_desc");
	job_desc = lua_touserdata(L, -1);
	if (job_desc == NULL) {
		error("%s: job_desc is NULL", __func__);
	} else {
		value_str = luaL_checkstring(L, 3);
		for (i = 0; job_desc->environment[i]; i++) {
			if (!xstrncmp(job_desc->environment[i], name_eq,
				      name_len)) {
				job_desc->environment[i][name_len] = '\0';
				xstrcat(job_desc->environment[i], value_str);
				break;
			}
		}
		if (!job_desc->environment[i]) {
			job_desc->environment = xrealloc(job_desc->environment,
							 sizeof(char*) * (i+2));
			for (j = i; j >= 1; j--) {
				job_desc->environment[j] =
					job_desc->environment[j-1];
			}
			job_desc->environment[0] = xstrdup(name_eq);
			xstrcat(job_desc->environment[0], value_str);
		}
	}
	xfree(name_eq);

	return 0;
}

static int _job_env_field(const struct job_descriptor *job_desc,
			  const char *name)
{
	char *name_eq = "";
	int i, name_len;

	name_eq = xstrdup(name);
	xstrcat(name_eq, "=");
	name_len = strlen(name_eq);
	if (job_desc == NULL) {
		error("%s: job_desc is NULL", __func__);
		lua_pushnil (L);
	} else if (job_desc->environment == NULL) {
		error("%s: job_desc->environment is NULL", __func__);
		lua_pushnil (L);
	} else {
		for (i = 0; job_desc->environment[i]; i++) {
			if (!xstrncmp(job_desc->environment[i], name_eq,
				      name_len)) {
				lua_pushstring (L, job_desc->environment[i] +
						   name_len);
				break;
			}
		}
		if (!job_desc->environment[i])
			lua_pushnil (L);
	}
	xfree(name_eq);

	return 1;
}

/* Get fields in the job request record on job submit or modify */
static int _get_job_env_field_name(lua_State *L)
{
	const struct job_descriptor *job_desc = lua_touserdata(L, 1);
	const char *name = luaL_checkstring(L, 2);
	return _job_env_field(job_desc, name);
}

/* Get fields in an existing slurmctld job_descriptor record */
static int _job_env_field_index(lua_State *L)
{
	const char *name;
	struct job_descriptor *job_desc;

	name = luaL_checkstring(L, 2);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_job_desc");
	job_desc = lua_touserdata(L, -1);
	return _job_env_field(job_desc, name);
}

static void _push_job_env(struct job_descriptor *job_desc)
{
	lua_newtable(L);

	lua_newtable(L);
	lua_pushcfunction(L, _job_env_field_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, _set_job_env_field);
	lua_setfield(L, -2, "__newindex");
	/* Store the job descriptor in the metatable, so the index
	 * function knows which struct it's getting data for.
	 */
	lua_pushlightuserdata(L, job_desc);
	lua_setfield(L, -2, "_job_desc");
	lua_setmetatable(L, -2);
}
#endif

#if 0
static int _get_job_req_field(const struct job_descriptor *job_desc,
			      const char *name)
{
	int i;

	if (job_desc == NULL) {
		error("%s: job_desc is NULL", __func__);
		lua_pushnil (L);
	} else if (!xstrcmp(name, "account")) {
		lua_pushstring (L, job_desc->account);
	} else if (!xstrcmp(name, "acctg_freq")) {
		lua_pushstring (L, job_desc->acctg_freq);
	} else if (!xstrcmp(name, "admin_comment")) {
		lua_pushstring (L, job_desc->admin_comment);
	} else if (!xstrcmp(name, "alloc_node")) {
		lua_pushstring (L, job_desc->alloc_node);
	} else if (!xstrcmp(name, "array_inx")) {
		lua_pushstring (L, job_desc->array_inx);
	} else if (!xstrcmp(name, "begin_time")) {
		lua_pushnumber (L, job_desc->begin_time);
	} else if (!xstrcmp(name, "bitflags")) {
		lua_pushnumber (L, job_desc->bitflags);
	} else if (!xstrcmp(name, "boards_per_node")) {
		lua_pushnumber (L, job_desc->boards_per_node);
	} else if (!xstrcmp(name, "burst_buffer")) {
		lua_pushstring (L, job_desc->burst_buffer);
	} else if (!xstrcmp(name, "clusters")) {
		lua_pushstring (L, job_desc->clusters);
	} else if (!xstrcmp(name, "comment")) {
		lua_pushstring (L, job_desc->comment);
	} else if (!xstrcmp(name, "contiguous")) {
		lua_pushnumber (L, job_desc->contiguous);
	} else if (!xstrcmp(name, "cores_per_socket")) {
		lua_pushnumber (L, job_desc->cores_per_socket);
	} else if (!xstrcmp(name, "cpu_freq_min")) {
		lua_pushnumber (L, job_desc->cpu_freq_min);
	} else if (!xstrcmp(name, "cpu_freq_max")) {
		lua_pushnumber (L, job_desc->cpu_freq_max);
	} else if (!xstrcmp(name, "cpu_freq_gov")) {
		lua_pushnumber (L, job_desc->cpu_freq_gov);
	} else if (!xstrcmp(name, "cpus_per_task")) {
		lua_pushnumber (L, job_desc->cpus_per_task);
	} else if (!xstrcmp(name, "default_account")) {
		lua_pushstring (L, _get_default_account(job_desc->user_id));
	} else if (!xstrcmp(name, "default_qos")) {
		lua_pushstring (L, _get_default_qos(job_desc->user_id,
						    job_desc->account,
						    job_desc->partition));
	} else if (!xstrcmp(name, "delay_boot")) {
		lua_pushnumber (L, job_desc->delay_boot);
	} else if (!xstrcmp(name, "dependency")) {
		lua_pushstring (L, job_desc->dependency);
	} else if (!xstrcmp(name, "end_time")) {
		lua_pushnumber (L, job_desc->end_time);
	} else if (!xstrcmp(name, "environment")) {
		_push_job_env ((struct job_descriptor *)job_desc); // No const
	} else if (!xstrcmp(name, "exc_nodes")) {
		lua_pushstring (L, job_desc->exc_nodes);
	} else if (!xstrcmp(name, "features")) {
		lua_pushstring (L, job_desc->features);
	} else if (!xstrcmp(name, "gres")) {
		lua_pushstring (L, job_desc->gres);
	} else if (!xstrcmp(name, "group_id")) {
		lua_pushnumber (L, job_desc->group_id);
	} else if (!xstrcmp(name, "immediate")) {
		lua_pushnumber (L, job_desc->immediate);
	} else if (!xstrcmp(name, "licenses")) {
		lua_pushstring (L, job_desc->licenses);
	} else if (!xstrcmp(name, "mail_type")) {
		lua_pushnumber (L, job_desc->mail_type);
	} else if (!xstrcmp(name, "mail_user")) {
		lua_pushstring (L, job_desc->mail_user);
	} else if (!xstrcmp(name, "max_cpus")) {
		lua_pushnumber (L, job_desc->max_cpus);
	} else if (!xstrcmp(name, "max_nodes")) {
		lua_pushnumber (L, job_desc->max_nodes);
	} else if (!xstrcmp(name, "min_cpus")) {
		lua_pushnumber (L, job_desc->min_cpus);
	} else if (!xstrcmp(name, "min_nodes")) {
		lua_pushnumber (L, job_desc->min_nodes);
	} else if (!xstrcmp(name, "name")) {
		lua_pushstring (L, job_desc->name);
	} else if (!xstrcmp(name, "nice")) {
		lua_pushnumber (L, job_desc->nice);
	} else if (!xstrcmp(name, "ntasks_per_board")) {
		lua_pushnumber (L, job_desc->ntasks_per_board);
	} else if (!xstrcmp(name, "ntasks_per_core")) {
		lua_pushnumber (L, job_desc->ntasks_per_core);
	} else if (!xstrcmp(name, "ntasks_per_node")) {
		lua_pushnumber (L, job_desc->ntasks_per_node);
	} else if (!xstrcmp(name, "ntasks_per_socket")) {
		lua_pushnumber (L, job_desc->ntasks_per_socket);
	} else if (!xstrcmp(name, "num_tasks")) {
		lua_pushnumber (L, job_desc->num_tasks);
	} else if (!xstrcmp(name, "partition")) {
		lua_pushstring (L, job_desc->partition);
	} else if (!xstrcmp(name, "power_flags")) {
		lua_pushnumber (L, job_desc->power_flags);
	} else if (!xstrcmp(name, "pn_min_cpus")) {
		lua_pushnumber (L, job_desc->pn_min_cpus);
	} else if (!xstrcmp(name, "pn_min_memory")) {
		lua_pushnumber (L, job_desc->pn_min_memory);
	} else if (!xstrcmp(name, "pn_min_tmp_disk")) {
		lua_pushnumber (L, job_desc->pn_min_tmp_disk);
	} else if (!xstrcmp(name, "priority")) {
		lua_pushnumber (L, job_desc->priority);
	} else if (!xstrcmp(name, "qos")) {
		lua_pushstring (L, job_desc->qos);
	} else if (!xstrcmp(name, "reboot")) {
		lua_pushnumber (L, job_desc->reboot);
	} else if (!xstrcmp(name, "req_nodes")) {
		lua_pushstring (L, job_desc->req_nodes);
	} else if (!xstrcmp(name, "req_switch")) {
		lua_pushnumber (L, job_desc->req_switch);
	} else if (!xstrcmp(name, "requeue")) {
		lua_pushnumber (L, job_desc->requeue);
	} else if (!xstrcmp(name, "reservation")) {
		lua_pushstring (L, job_desc->reservation);
	} else if (!xstrcmp(name, "script")) {
		lua_pushstring (L, job_desc->script);
	} else if (!xstrcmp(name, "shared")) {
		lua_pushnumber (L, job_desc->shared);
	} else if (!xstrcmp(name, "sockets_per_board")) {
		lua_pushnumber (L, job_desc->sockets_per_board);
	} else if (!xstrcmp(name, "sockets_per_node")) {
		lua_pushnumber (L, job_desc->sockets_per_node);
	} else if (!xstrcmp(name, "spank_job_env")) {
		if ((job_desc->spank_job_env_size == 0) ||
		    (job_desc->spank_job_env == NULL)) {
			lua_pushnil (L);
		} else {
			lua_newtable(L);
			for (i = 0; i < job_desc->spank_job_env_size; i++) {
				if (job_desc->spank_job_env[i] != NULL) {
					lua_pushnumber (L, i);
					lua_pushstring (L,
						job_desc->spank_job_env[i]);
					lua_settable (L, -3);
				}
			}
		}
	} else if (!xstrcmp(name, "spank_job_env_size")) {
		lua_pushnumber (L, job_desc->spank_job_env_size);
	} else if (!xstrcmp(name, "std_err")) {
		lua_pushstring (L, job_desc->std_err);
	} else if (!xstrcmp(name, "std_in")) {
		lua_pushstring (L, job_desc->std_in);
	} else if (!xstrcmp(name, "std_out")) {
		lua_pushstring (L, job_desc->std_out);
	} else if (!xstrcmp(name, "threads_per_core")) {
		lua_pushnumber (L, job_desc->threads_per_core);
	} else if (!xstrcmp(name, "time_limit")) {
		lua_pushnumber (L, job_desc->time_limit);
	} else if (!xstrcmp(name, "time_min")) {
		lua_pushnumber (L, job_desc->time_min);
	} else if (!xstrcmp(name, "user_id")) {
		lua_pushnumber (L, job_desc->user_id);
	} else if (!xstrcmp(name, "wait4switch")) {
		lua_pushnumber (L, job_desc->wait4switch);
	} else if (!xstrcmp(name, "work_dir")) {
		lua_pushstring (L, job_desc->work_dir);
	} else if (!xstrcmp(name, "wckey")) {
		lua_pushstring (L, job_desc->wckey);
	} else {
		lua_pushnil (L);
	}

	return 1;
}

/* Get fields in the job request record on job submit or modify */
static int _get_job_req_field_name(lua_State *L)
{
	const struct job_descriptor *job_desc = lua_touserdata(L, 1);
	const char *name = luaL_checkstring(L, 2);

	return _get_job_req_field(job_desc, name);
}

/* Get fields in an existing slurmctld job_descriptor record */
static int _get_job_req_field_index(lua_State *L)
{
	const char *name;
	struct job_descriptor *job_desc;

	name = luaL_checkstring(L, 2);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_job_desc");
	job_desc = lua_touserdata(L, -1);

	return _get_job_req_field(job_desc, name);
}

/* Set fields in the job request structure on job submit or modify */
static int _set_job_req_field(lua_State *L)
{
	const char *name, *value_str;
	struct job_descriptor *job_desc;

	name = luaL_checkstring(L, 2);
	lua_getmetatable(L, -3);
	lua_getfield(L, -1, "_job_desc");
	job_desc = lua_touserdata(L, -1);
	if (job_desc == NULL) {
		error("%s: job_desc is NULL", __func__);
	} else if (!xstrcmp(name, "account")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->account);
		if (strlen(value_str))
			job_desc->account = xstrdup(value_str);
	} else if (!xstrcmp(name, "acctg_freq")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->acctg_freq);
		if (strlen(value_str))
			job_desc->acctg_freq = xstrdup(value_str);
	} else if (!xstrcmp(name, "admin_comment")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->admin_comment);
		if (strlen(value_str))
			job_desc->admin_comment = xstrdup(value_str);
	} else if (!xstrcmp(name, "array_inx")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->array_inx);
		if (strlen(value_str))
			job_desc->array_inx = xstrdup(value_str);
	} else if (!xstrcmp(name, "begin_time")) {
		job_desc->begin_time = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "bitflags")) {
		job_desc->bitflags = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "burst_buffer")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->burst_buffer);
		if (strlen(value_str))
			job_desc->burst_buffer = xstrdup(value_str);
	} else if (!xstrcmp(name, "clusters")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->clusters);
		if (strlen(value_str))
			job_desc->clusters = xstrdup(value_str);
	} else if (!xstrcmp(name, "comment")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->comment);
		if (strlen(value_str))
			job_desc->comment = xstrdup(value_str);
	} else if (!xstrcmp(name, "contiguous")) {
		job_desc->contiguous = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "cores_per_socket")) {
		job_desc->cores_per_socket = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "cpus_per_task")) {
		job_desc->cpus_per_task = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "cpu_freq_min")) {
		job_desc->cpu_freq_min = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "cpu_freq_max")) {
		job_desc->cpu_freq_max = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "cpu_freq_gov")) {
		job_desc->cpu_freq_gov = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "dependency")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->dependency);
		if (strlen(value_str))
			job_desc->dependency = xstrdup(value_str);
	} else if (!xstrcmp(name, "delay_boot")) {
		job_desc->delay_boot = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "end_time")) {
		job_desc->end_time = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "exc_nodes")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->exc_nodes);
		if (strlen(value_str))
			job_desc->exc_nodes = xstrdup(value_str);
	} else if (!xstrcmp(name, "features")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->features);
		if (strlen(value_str))
			job_desc->features = xstrdup(value_str);
	} else if (!xstrcmp(name, "gres")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->gres);
		if (strlen(value_str))
			job_desc->gres = xstrdup(value_str);
	} else if (!xstrcmp(name, "immediate")) {
		job_desc->immediate = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "licenses")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->licenses);
		if (strlen(value_str))
			job_desc->licenses = xstrdup(value_str);
	} else if (!xstrcmp(name, "max_cpus")) {
		job_desc->max_cpus = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "max_nodes")) {
		job_desc->max_nodes = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "min_cpus")) {
		job_desc->min_cpus = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "min_nodes")) {
		job_desc->min_nodes = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "name")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->name);
		if (strlen(value_str))
			job_desc->name = xstrdup(value_str);
	} else if (!xstrcmp(name, "nice")) {
		job_desc->nice = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "ntasks_per_node")) {
		job_desc->ntasks_per_node = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "ntasks_per_socket")) {
		job_desc->ntasks_per_socket = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "num_tasks")) {
		job_desc->num_tasks = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "partition")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->partition);
		if (strlen(value_str))
			job_desc->partition = xstrdup(value_str);
	} else if (!xstrcmp(name, "power_flags")) {
		job_desc->power_flags = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "pn_min_cpus")) {
		job_desc->pn_min_cpus = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "pn_min_memory")) {
		job_desc->pn_min_memory = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "pn_min_tmp_disk")) {
		job_desc->pn_min_tmp_disk = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "priority")) {
		job_desc->priority = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "qos")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->qos);
		if (strlen(value_str))
			job_desc->qos = xstrdup(value_str);
	} else if (!xstrcmp(name, "reboot")) {
		job_desc->reboot = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "req_nodes")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->req_nodes);
		if (strlen(value_str))
			job_desc->req_nodes = xstrdup(value_str);
	} else if (!xstrcmp(name, "req_switch")) {
		job_desc->req_switch = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "requeue")) {
		job_desc->requeue = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "reservation")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->reservation);
		if (strlen(value_str))
			job_desc->reservation = xstrdup(value_str);
	} else if (!xstrcmp(name, "script")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->script);
		if (strlen(value_str))
			job_desc->script = xstrdup(value_str);
	} else if (!xstrcmp(name, "shared")) {
		job_desc->shared = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "sockets_per_node")) {
		job_desc->sockets_per_node = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "std_err")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->std_err);
		if (strlen(value_str))
			job_desc->std_err = xstrdup(value_str);
	} else if (!xstrcmp(name, "std_in")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->std_in);
		if (strlen(value_str))
			job_desc->std_in = xstrdup(value_str);
	} else if (!xstrcmp(name, "std_out")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->std_out);
		if (strlen(value_str))
			job_desc->std_out = xstrdup(value_str);
	} else if (!xstrcmp(name, "threads_per_core")) {
		job_desc->threads_per_core = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "time_limit")) {
		job_desc->time_limit = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "time_min")) {
		job_desc->time_min = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "wait4switch")) {
		job_desc->wait4switch = luaL_checknumber(L, 3);
	} else if (!xstrcmp(name, "wckey")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->wckey);
		if (strlen(value_str))
			job_desc->wckey = xstrdup(value_str);
	} else if (!xstrcmp(name, "work_dir")) {
		value_str = luaL_checkstring(L, 3);
		xfree(job_desc->work_dir);
		if (strlen(value_str))
			job_desc->work_dir = xstrdup(value_str);
	} else {
		error("_set_job_field: unrecognized field: %s", name);
	}

	return 0;
}

static void _push_job_desc(struct job_descriptor *job_desc)
{
	lua_newtable(L);

	lua_newtable(L);
	lua_pushcfunction(L, _get_job_req_field_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, _set_job_req_field);
	lua_setfield(L, -2, "__newindex");
	/* Store the job descriptor in the metatable, so the index
	 * function knows which struct it's getting data for.
	 */
	lua_pushlightuserdata(L, job_desc);
	lua_setfield(L, -2, "_job_desc");
	lua_setmetatable(L, -2);
}

static void _push_job_rec(struct job_record *job_ptr)
{
	lua_newtable(L);

	lua_newtable(L);
	lua_pushcfunction(L, _job_rec_field_index);
	lua_setfield(L, -2, "__index");
	/* Store the job_ptr in the metatable, so the index
	 * function knows which struct it's getting data for.
	 */
	lua_pushlightuserdata(L, job_ptr);
	lua_setfield(L, -2, "_job_rec_ptr");
	lua_setmetatable(L, -2);
}

/* Get fields in an existing slurmctld partition record
 *
 * This is an incomplete list of partition record fields. Add more as needed
 * and send patches to slurm-dev@schedmd.com
 */
static int _part_rec_field(const struct part_record *part_ptr,
                           const char *name)
{
	if (part_ptr == NULL) {
		error("_get_part_field: part_ptr is NULL");
		lua_pushnil (L);
	} else if (!xstrcmp(name, "allow_qos")) {
		lua_pushstring (L, part_ptr->allow_qos);
	} else if (!xstrcmp(name, "default_time")) {
		lua_pushnumber (L, part_ptr->default_time);
	} else if (!xstrcmp(name, "flag_default")) {
		int is_default = 0;
		if (part_ptr->flags & PART_FLAG_DEFAULT)
			is_default = 1;
		lua_pushnumber (L, is_default);
	} else if (!xstrcmp(name, "flags")) {
		lua_pushnumber (L, part_ptr->flags);
	} else if (!xstrcmp(name, "max_nodes")) {
		lua_pushnumber (L, part_ptr->max_nodes);
	} else if (!xstrcmp(name, "max_nodes_orig")) {
		lua_pushnumber (L, part_ptr->max_nodes_orig);
	} else if (!xstrcmp(name, "max_time")) {
		lua_pushnumber (L, part_ptr->max_time);
	} else if (!xstrcmp(name, "min_nodes")) {
		lua_pushnumber (L, part_ptr->min_nodes);
	} else if (!xstrcmp(name, "min_nodes_orig")) {
		lua_pushnumber (L, part_ptr->min_nodes_orig);
	} else if (!xstrcmp(name, "name")) {
		lua_pushstring (L, part_ptr->name);
	} else if (!xstrcmp(name, "nodes")) {
		lua_pushstring (L, part_ptr->nodes);
	} else if (!xstrcmp(name, "priority_job_factor")) {
		lua_pushnumber (L, part_ptr->priority_job_factor);
	} else if (!xstrcmp(name, "priority_tier")) {
		lua_pushnumber (L, part_ptr->priority_tier);
	} else if (!xstrcmp(name, "qos")) {
		lua_pushstring (L, part_ptr->qos_char);
	} else if (!xstrcmp(name, "state_up")) {
		lua_pushnumber (L, part_ptr->state_up);
	} else {
		lua_pushnil (L);
	}

	return 1;
}

static int _get_part_rec_field (lua_State *L)
{
	const struct part_record *part_ptr = lua_touserdata(L, 1);
	const char *name = luaL_checkstring(L, 2);

	return _part_rec_field(part_ptr, name);
}

static int _part_rec_field_index(lua_State *L)
{
	const char *name = luaL_checkstring(L, 2);
	struct part_record *part_ptr;

	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_part_rec_ptr");
	part_ptr = lua_touserdata(L, -1);

	return _part_rec_field(part_ptr, name);
}

static bool _user_can_use_part(uint32_t user_id, uint32_t submit_uid,
			       struct part_record *part_ptr)
{
	int i;

	if (user_id == 0) {
		if (part_ptr->flags & PART_FLAG_NO_ROOT)
			return false;
		return true;
	}

	if ((part_ptr->flags & PART_FLAG_ROOT_ONLY) && (submit_uid != 0))
		return false;

	if (part_ptr->allow_uids == NULL)
		return true;	/* No user ID filters */

	for (i=0; part_ptr->allow_uids[i]; i++) {
		if (user_id == part_ptr->allow_uids[i])
			return true;
	}
	return false;
}

static void _push_partition_list(uint32_t user_id, uint32_t submit_uid)
{
	ListIterator part_iterator;
	struct part_record *part_ptr;

	lua_newtable(L);
	part_iterator = list_iterator_create(part_list);
	while ((part_ptr = (struct part_record *) list_next(part_iterator))) {
		if (!_user_can_use_part(user_id, submit_uid, part_ptr))
			continue;

		/* Create an empty table, with a metatable that looks up the
		 * data for the partition.
		 */
		lua_newtable(L);

		lua_newtable(L);
		lua_pushcfunction(L, _part_rec_field_index);
		lua_setfield(L, -2, "__index");
		/* Store the part_record in the metatable, so the index
		 * function knows which job it's getting data for.
		 */
		lua_pushlightuserdata(L, part_ptr);
		lua_setfield(L, -2, "_part_rec_ptr");
		lua_setmetatable(L, -2);

		lua_setfield(L, -2, part_ptr->name);
	}
	list_iterator_destroy(part_iterator);
}

#endif
