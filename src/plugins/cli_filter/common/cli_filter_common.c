/*****************************************************************************\
 *  cli_filter_common.c - Common infrastructure available to all cli_filter
 *****************************************************************************
 *  Copyright (C) 2017 Regents of the University of California
 *  Produced at Lawrence Berkeley National Laboratory (cf, DISCLAIMER).
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

#include "cli_filter_common.h"
#include "src/common/cli_filter.h"
#include <src/common/xstring.h>
#include <src/common/xmalloc.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>

/* in the future this function and the option_string data structures should
 * probably be converted to use a binary search or other faster method for
 * discovering the requested option */
const cli_string_option_t *cli_si_find(const char *name,
                                       const cli_string_option_t *opt)
{
	size_t idx = 0;
	for (idx = 0; opt[idx].name; idx++)
		if (!xstrcmp(opt[idx].name, name)) {
			return &(opt[idx]);
		}
	return NULL;
}

static const cli_string_option_t *_lookup_client(int client_id) {
	if (client_id == CLI_SALLOC)
		return salloc_options_si;
	if (client_id == CLI_SBATCH)
		return sbatch_options_si;
	if (client_id == CLI_SRUN)
		return srun_options_si;
	if (client_id == CLI_SBCAST)
		return bcast_parameters_si;
	return NULL;
}

char *cli_si_get(const char *key, void *data, int client_id)
{
	if (!key || !data)
		return NULL;

	const cli_string_option_t *opt = _lookup_client(client_id);
	const cli_string_option_t *found = cli_si_find(key, opt);
	if (!found)
		return NULL;

	return (found->read)(data, found);
}

bool cli_si_set(const char *value, const char *key, void *data, int client_id)
{
	if (!value || !key || !data)
		return NULL;

	const cli_string_option_t *opt = _lookup_client(client_id);
	const cli_string_option_t *found = cli_si_find(key, opt);
	return (found->write)(value, data, found);
}

static char *_json_escape(const char *str) {
	char *ret = NULL;
	int i;
	for (i = 0; i < strlen(str); ++i) {
		switch (str[i]) {
			case '\\':
				xstrcat(ret, "\\\\");
				break;
			case '"':
				xstrcat(ret, "\\\"");
				break;
			case '\n':
				xstrcat(ret, "\\n");
				break;
			case '\b':
				xstrcat(ret, "\\b");
				break;
			case '\f':
				xstrcat(ret, "\\f");
				break;
			case '\r':
				xstrcat(ret, "\\r");
				break;
			case '\t':
				xstrcat(ret, "\\t");
				break;
			case '<':
				xstrcat(ret, "\\u003C");
				break;
			default:
				xstrcatchar(ret, str[i]);
		}
	}
	return ret;
}

char *cli_gen_json(uint32_t jobid, void *data, int client_id) {
	const cli_string_option_t *opt = _lookup_client(client_id);
	size_t idx = 0;
	char *json = xmalloc(2048);
	xstrfmtcat(json, "{\"job_id\":%"PRIu32, jobid);
	for (idx = 0; opt[idx].name; idx++) {
		char *value = cli_si_get(opt[idx].name, data, client_id);
		char *esc = NULL;
		if (!value)
			continue;
		esc = _json_escape(value);
		xstrfmtcat(json, ",\"%s\":\"%s\"", opt[idx].name, esc);
		xfree(value);
		xfree(esc);
		
	}
	xstrcat(json, "}");
	return json;
}


char *cli_si_get_string(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        char **tgt = (char **) (data + stropt->offset);
        char *ret = xstrdup(*tgt);
        return ret;
}

bool cli_si_set_string(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !stropt || !value)
                return false;

        char **tgt = (char **) (data + stropt->offset);
        xfree(*tgt);
        *tgt = xstrdup(value);
        return true;
}

char *cli_si_get_stringarray(void *data, const cli_string_option_t *stropt)
{
	if (!data || !stropt)
		return NULL;
	char ***tgt = (char ***) (data + stropt->offset);
	char *ret = xmalloc(1024);
	char **arg = *tgt;
	for (arg = *tgt; arg && *arg; arg++) {
		xstrfmtcat(ret, "%s|", *arg);
	}
	return ret;
}

char *cli_si_get_int(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        int *tgt = (int *) (data + stropt->offset);
        return xstrdup_printf("%d", *tgt);
}

bool cli_si_set_int(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        int *tgt = (int *) (data + stropt->offset);
        int towrite = atoi(value);
        *tgt = towrite;
        return true;
}

char *cli_si_get_long(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        long *tgt = (long *) (data + stropt->offset);
        return xstrdup_printf("%ld", *tgt);
}

bool cli_si_set_long(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        long *tgt = (long *) (data + stropt->offset);
        long towrite = atol(value);
        *tgt = towrite;
        return true;
}

char *cli_si_get_unsigned(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        unsigned *tgt = (unsigned *) (data + stropt->offset);
        return xstrdup_printf("%u", *tgt);
}

bool cli_si_set_unsigned(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        unsigned *tgt = (unsigned *) (data + stropt->offset);
        unsigned towrite = (unsigned) strtoul(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_uid(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        uid_t *tgt = (uid_t *) (data + stropt->offset);
        return xstrdup_printf("%d", *tgt);
}

char *cli_si_get_gid(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        gid_t *tgt = (gid_t *) (data + stropt->offset);
        return xstrdup_printf("%d", *tgt);
}

char *cli_si_get_int8_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        int8_t *tgt = (int8_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRId8, *tgt);
}

bool cli_si_set_int8_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        int8_t *tgt = (int8_t *) (data + stropt->offset);
        int8_t towrite = (int8_t) strtol(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_int16_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        int16_t *tgt = (int16_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRId16, *tgt);
}

bool cli_si_set_int16_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        int16_t *tgt = (int16_t *) (data + stropt->offset);
        int16_t towrite = (int16_t) strtol(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_int32_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        int32_t *tgt = (int32_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRId32, *tgt);
}

bool cli_si_set_int32_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        int32_t *tgt = (int32_t *) (data + stropt->offset);
        int32_t towrite = (int32_t) strtol(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_int64_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        int64_t *tgt = (int64_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRId64, *tgt);
}

bool cli_si_set_int64_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        int64_t *tgt = (int64_t *) (data + stropt->offset);
        int64_t towrite = (int64_t) strtoll(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_uint8_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        uint8_t *tgt = (uint8_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRIu8, *tgt);
}

bool cli_si_set_uint8_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        uint8_t *tgt = (uint8_t *) (data + stropt->offset);
        uint8_t towrite = (uint8_t) strtoul(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_uint16_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        uint16_t *tgt = (uint16_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRIu16, *tgt);
}

bool cli_si_set_uint16_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        uint16_t *tgt = (uint16_t *) (data + stropt->offset);
        uint16_t towrite = (uint16_t) strtoul(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_uint32_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        uint32_t *tgt = (uint32_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRIu32, *tgt);
}

bool cli_si_set_uint32_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        uint32_t *tgt = (uint32_t *) (data + stropt->offset);
        uint32_t towrite = (uint32_t) strtoul(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_uint64_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        uint64_t *tgt = (uint64_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRIu64, *tgt);
}

bool cli_si_set_uint64_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        uint64_t *tgt = (uint64_t *) (data + stropt->offset);
        uint64_t towrite = (uint64_t) strtoull(value, NULL, 10);
        *tgt = towrite;
        return true;
}

char *cli_si_get_boolean(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        bool *tgt = (bool *) (data + stropt->offset);
        return xstrdup_printf("%s", *tgt ? "true" : "false");
}

bool cli_si_set_boolean(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        bool *tgt = (bool *) (data + stropt->offset);
	bool towrite = false;
	if (!strcasecmp(value, "true") || atoi(value))
		towrite = true;
        *tgt = towrite;
        return true;
}

char *cli_si_get_time_t(void *data, const cli_string_option_t *stropt)
{
        if (!data || !stropt)
                return NULL;
        time_t *tgt = (time_t *) (data + stropt->offset);
        return xstrdup_printf("%"PRId64, (int64_t) *tgt);
}

bool cli_si_set_time_t(const char *value, void *data,
		const cli_string_option_t *stropt)
{
        if (!data || !value || !stropt)
                return false;

        time_t *tgt = (time_t *) (data + stropt->offset);
        time_t towrite = (time_t) strtoul(value, NULL, 10);
        *tgt = towrite;
	return true;
}
