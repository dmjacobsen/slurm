/*****************************************************************************\
 *  cli_filter_lua.c - lua CLI option processing specifications.
 *****************************************************************************
 *  Copyright (C) 2017-2019 Regents of the University of California
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
#include "src/plugins/cli_filter/common/cli_filter_common.h"
#include "src/common/slurm_opt.h"
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
static char **stored_data = NULL;
static size_t stored_n = 0;
static size_t stored_sz = 0;

static int _log_lua_msg(lua_State *L);
static int _log_lua_error(lua_State *L);
static int _log_lua_user_msg(lua_State *L);
static int _lua_cli_json(lua_State *L);
static int _lua_cli_json_env(lua_State *L);
static int _store_data(lua_State *L);
static int _retrieve_data(lua_State *L);
static int _load_script(void);
static void _stack_dump (char *header, lua_State *L);

static const struct luaL_Reg slurm_functions[] = {
	{ "log",	_log_lua_msg   },
	{ "error",	_log_lua_error },
	{ "user_msg",	_log_lua_user_msg },
	{ "json_cli_options",_lua_cli_json },
	{ "json_env",	_lua_cli_json_env },
	{ "cli_store",	_store_data },
	{ "cli_retrieve", _retrieve_data },
	{ NULL,		NULL        }
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

	stored_data = xmalloc(sizeof(char *) * 24);
	stored_sz = 24;

        return _load_script();
}

int fini(void)
{
	for (int i = 0; i < stored_n; i++)
		xfree(stored_data[i]);
	xfree(stored_data);

        lua_close (L);
        return SLURM_SUCCESS;
}

static int _setup_stringarray(lua_State *L, int limit, char **data) {

	/*
	 * if limit/data empty this will create an empty array intentionally to
         * allow the client code to iterate over it
	 */
	lua_newtable(L);
	for (int i = 0; i < limit && data && data[i]; i++) {
		/* lua indexes tables from 1 */
		lua_pushnumber(L, i + 1);
		lua_pushstring(L, data[i]);
		lua_settable(L, -3);
	}
	return 1;
}

static int _get_option_field_argv(lua_State *L, slurm_opt_t *opt)
{
	char **argv = NULL;
	int  argc = 0;

	if (opt->sbatch_opt) {
		argv = opt->sbatch_opt->script_argv;
		argc = opt->sbatch_opt->script_argc;
	} else if (opt->srun_opt) {
		argv = opt->srun_opt->argv;
		argc = opt->srun_opt->argc;
	}

	return _setup_stringarray(L, argc, argv);
}

static int _get_option_field_index(lua_State *L)
{
	const char *name;
	slurm_opt_t *options = NULL;
	char *value = NULL;

	name = luaL_checkstring(L, -1);
	lua_getmetatable(L, -2);
	lua_getfield(L, -1, "_opt");
	options = (slurm_opt_t *) lua_touserdata(L, -1);

	/* getmetatable and getfield pushed two items onto the stack above
	 * get rid of them here, and leave the name string at the top of
	 * the stack */
	lua_settop(L, -3);

	/* specially handle "argv" and "spank_job_env" */
	if (strcmp(name, "argv") == 0)
		return _get_option_field_argv(L, options);
	else if (strcmp(name, "spank_job_env") == 0)
		return _setup_stringarray(L, options->spank_job_env_size,
					  options->spank_job_env);

	value = slurm_option_get(options, name);
	if (!value)
		lua_pushnil(L);
	else
		lua_pushstring(L, value);
	xfree(value);
	return 1;
}

static int _set_option_field(lua_State *L)
{
	slurm_opt_t *options = NULL;
	const char *name = NULL;
	const char *value = NULL;
	bool early = false;

	_stack_dump("cli_filter, early _set_option_field", L);
	name = luaL_checkstring(L, -2);
	value = luaL_checkstring(L, -1);
	lua_getmetatable(L, -3);
	lua_getfield(L, -1, "_opt");
	options = (slurm_opt_t *) lua_touserdata(L, -1);
	lua_getfield(L, -2, "_early");
	early = lua_toboolean(L, -1);

	/* getmetatable and getfields pushed three items onto the stack above
	 * get rid of them here, and leave the name string at the top of
	 * the stack */
	lua_settop(L, -4);

	if (slurm_option_set(options, name, value, early))
		return 1;
	return 0;
}

static void _push_options(slurm_opt_t *opt, bool early)
{
	lua_newtable(L); /* table to push */

	lua_newtable(L); /* metatable */
	lua_pushcfunction(L, _get_option_field_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, _set_option_field);
	lua_setfield(L, -2, "__newindex");
	/* Store opt in the metatable, so the index
	 * function knows which struct it's getting data for.
	 */
	lua_pushlightuserdata(L, opt);
	lua_setfield(L, -2, "_opt");
	lua_pushboolean(L, early);
	lua_setfield(L, -2, "_early");
	lua_setmetatable(L, -2);
}

static int _log_lua_error(lua_State *L)
{
	const char *prefix  = "cli_filter/lua";
	const char *msg     = lua_tostring (L, -1);
	error ("%s: %s", prefix, msg);
	return (0);
}

static int _log_lua_user_msg(lua_State *L)
{
	const char *msg = lua_tostring(L, -1);

	xfree(user_msg);
	user_msg = xstrdup(msg);
	return (0);
}

static int _lua_cli_json(lua_State *L)
{
	char *json = NULL;
	slurm_opt_t *options = NULL;
	lua_getmetatable(L, -1);
	lua_getfield(L, -1, "_opt");
	options = (slurm_opt_t *) lua_touserdata(L, -1);
	lua_settop(L, -3);

	json = cli_filter_json_set_options(options);
	if (json)
		lua_pushstring(L, json);
	else
		lua_pushnil(L);
	xfree(json);
	return 1;
}

static int _lua_cli_json_env(lua_State *L)
{
	char *output = cli_filter_json_env();
	lua_pushstring(L, output);
	xfree(output);
	return 1;
}

static int _store_data(lua_State *L)
{
	int key = 0;
	const char *data = NULL;

	key = (int) lua_tonumber(L, -2);
	data = luaL_checkstring(L, -1);

	if (key >= stored_sz) {
		stored_data = xrealloc(stored_data, (key + 24) * sizeof(char *));
		stored_sz = key + 24;
	}
	if (key > stored_n)
		stored_n = key;
	stored_data[key] = xstrdup(data);
	return 0;
}

static int _retrieve_data(lua_State *L)
{
	int key = (int) lua_tonumber(L, -1);
	if (key <= stored_n && stored_data[key])
		lua_pushstring(L, stored_data[key]);
	else
		lua_pushnil(L);
	return 1;
}

/*
 *  Lua interface to Slurm log facility:
 */
static int _log_lua_msg(lua_State *L)
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
		"slurm_cli_setup_defaults",
		"slurm_cli_pre_submit",
		"slurm_cli_post_submit",
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
	 *  Create more user-friendly lua versions of Slurm log functions.
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
	 * Error codes: slurm.SUCCESS, slurm.ERROR, etc.
	 */
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
	 *  logging write functions
	 */
	_register_lua_slurm_output_functions();

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

extern int setup_defaults(slurm_opt_t *opt, bool early) {
	int rc = SLURM_ERROR;
	(void) _load_script();

	lua_getglobal(L, "slurm_cli_setup_defaults");
	if (lua_isnil(L, -1))
		goto out;
	_push_options(opt, early);
	if (lua_pcall(L, 1, 1, 0) != 0) {
		error("%s/lua: %s: %s", __func__, lua_script_path,
			lua_tostring(L, -1));
	} else {
		if (lua_isnumber(L, -1)) {
			rc = lua_tonumber(L, -1);
		} else {
			info("%s/lua: %s: non-numeric return code", __func__,
				lua_script_path);
			rc = SLURM_SUCCESS;
		}
		lua_pop(L, 1);
	}
	if (user_msg) {
		info("%s", user_msg);
		xfree(user_msg);
	}

out:	lua_close (L);
	return rc;
}

extern int pre_submit(slurm_opt_t *opt, int pack_offset)
{
	int rc = SLURM_ERROR;

	(void) _load_script();

	/*
	 *  All lua script functions should have been verified during
	 *   initialization:
	 */
	lua_getglobal(L, "slurm_cli_pre_submit");
	if (lua_isnil(L, -1))
		goto out;

	_push_options(opt, false);
	lua_pushnumber(L, (double) pack_offset);

	_stack_dump("cli_filter, before lua_pcall", L);
	if (lua_pcall (L, 2, 1, 0) != 0) {
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
		info("%s", user_msg);
		xfree(user_msg);
	}

out:	lua_close (L);
	return rc;
}

extern int post_submit(int pack_offset, uint32_t jobid, uint32_t stepid)
{
	int rc = SLURM_ERROR;
	(void) _load_script();

	lua_getglobal(L, "slurm_cli_post_submit");
	if (lua_isnil(L, -1))
		goto out;
	lua_pushnumber(L, (double) pack_offset);
	lua_pushnumber(L, (double) jobid);
	lua_pushnumber(L, (double) stepid);
	if (lua_pcall(L, 3, 1, 0) != 0) {
		error("%s/lua: %s: %s", __func__, lua_script_path,
			lua_tostring(L, -1));
	} else {
		if (lua_isnumber(L, -1)) {
			rc = lua_tonumber(L, -1);
		} else {
			info("%s/lua: %s: non-numeric return code", __func__,
				lua_script_path);
			rc = SLURM_SUCCESS;
		}
		lua_pop(L, 1);
	}
	if (user_msg) {
		info("%s", user_msg);
		xfree(user_msg);
	}

out:	lua_close (L);
	return rc;
}
