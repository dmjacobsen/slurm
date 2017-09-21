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

#ifndef _CLI_FILTER_LUA_H_
#define _CLI_FILTER_LUA_H_

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
#include "src/bcast/file_bcast.h"
#include "src/srun/libsrun/srun_opt.h"
#include "src/common/xlua.h"

typedef struct _lua_option {
	char *name;
	size_t offset;
	bool (*read)(void *, const char *, const struct _lua_option *,
			lua_State *);
	bool (*write)(void *, int, const char *, const struct _lua_option *,
			lua_State *);
	const char *count_field;
} cli_lua_option_t;

extern cli_lua_option_t salloc_options_li[];
extern cli_lua_option_t sbatch_options_li[];
extern cli_lua_option_t srun_options_li[];
extern cli_lua_option_t bcast_parameters_li[];

bool cli_lua_push_string(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_string(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_stringarray(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_int(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_int(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_long(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_long(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_unsigned(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_unsigned(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_uid(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_gid(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_int8_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_int8_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_int16_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_int16_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_int32_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_int32_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_int64_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_int64_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_uint8_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_uint8_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_uint16_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_uint16_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_uint32_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_uint32_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_uint64_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_uint64_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_boolean(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_boolean(void *, int, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_push_time_t(void *, const char *, const cli_lua_option_t *, lua_State *);
bool cli_lua_set_time_t(void *, int, const char *, const cli_lua_option_t *, lua_State *);

#endif
