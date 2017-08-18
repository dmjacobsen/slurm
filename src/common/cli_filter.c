/*****************************************************************************\
 *  cli_filter.c - driver for cli_filter plugin
 *****************************************************************************
 *  Copyright (C) 2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
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
#include <unistd.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/macros.h"
#include "src/common/plugin.h"
#include "src/common/plugrack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/timers.h"
#include "src/common/cli_filter.h"

typedef struct cli_filter_ops {
	int		(*pre_submit)	( int cli_type,
					  void *opt,
					  char **err_msg );
	int		(*post_submit)	( int cli_type,
					  uint32_t jobid,
					  void *opt,
					  char **err_msg );
} cli_filter_ops_t;

/*
 * Must be synchronized with cli_filter_ops_t above.
 */
static const char *syms[] = {
	"pre_submit",
	"post_submit"
};

static int g_context_cnt = -1;
static cli_filter_ops_t *ops = NULL;
static plugin_context_t **g_context = NULL;
static char *clifilter_plugin_list = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;

/*
 * Initialize the cli filter plugin.
 *
 * Returns a SLURM errno.
 */
extern int cli_filter_plugin_init(void)
{
	int rc = SLURM_SUCCESS;
	char *last = NULL, *names;
	char *plugin_type = "cli_filter";
	char *type;

	if (init_run && (g_context_cnt >= 0))
		return rc;

	slurm_mutex_lock(&g_context_lock);
	if (g_context_cnt >= 0)
		goto fini;

	clifilter_plugin_list = slurm_get_cli_filter_plugins();
	g_context_cnt = 0;
	if ((clifilter_plugin_list == NULL) || (clifilter_plugin_list[0] == '\0'))
		goto fini;

	names = clifilter_plugin_list;
	while ((type = strtok_r(names, ",", &last))) {
		xrealloc(ops,
			 (sizeof(cli_filter_ops_t) * (g_context_cnt + 1)));
		xrealloc(g_context,
			 (sizeof(plugin_context_t *) * (g_context_cnt + 1)));
		if (xstrncmp(type, "cli_filter/", 11) == 0)
			type += 11; /* backward compatibility */
		type = xstrdup_printf("cli_filter/%s", type);
		g_context[g_context_cnt] = plugin_context_create(
			plugin_type, type, (void **)&ops[g_context_cnt],
			syms, sizeof(syms));
		if (!g_context[g_context_cnt]) {
			error("cannot create %s context for %s",
			      plugin_type, type);
			rc = SLURM_ERROR;
			xfree(type);
			break;
		}

		xfree(type);
		g_context_cnt++;
		names = NULL; /* for next strtok_r() iteration */
	}
	init_run = true;

fini:
	slurm_mutex_unlock(&g_context_lock);

	if (rc != SLURM_SUCCESS)
		cli_filter_plugin_fini();

	return rc;
}

/*
 * Terminate the cli filter plugin. Free memory.
 *
 * Returns a SLURM errno.
 */
extern int cli_filter_plugin_fini(void)
{
	int i, j, rc = SLURM_SUCCESS;

	slurm_mutex_lock(&g_context_lock);
	if (g_context_cnt < 0)
		goto fini;

	init_run = false;
	for (i=0; i<g_context_cnt; i++) {
		if (g_context[i]) {
			j = plugin_context_destroy(g_context[i]);
			if (j != SLURM_SUCCESS)
				rc = j;
		}
	}
	xfree(ops);
	xfree(g_context);
	xfree(clifilter_plugin_list);
	g_context_cnt = -1;

fini:	slurm_mutex_unlock(&g_context_lock);
	return rc;
}

/*
 **************************************************************************
 *                          P L U G I N   C A L L S                       *
 **************************************************************************
 */

/*
 * Perform reconfig, re-read any configuration files
 */
extern int cli_filter_plugin_reconfig(void)
{
	int rc = SLURM_SUCCESS;
	char *plugin_names = slurm_get_cli_filter_plugins();
	bool plugin_change;

	if (!plugin_names && !clifilter_plugin_list)
		return rc;

	slurm_mutex_lock(&g_context_lock);
	if (plugin_names && clifilter_plugin_list &&
	    xstrcmp(plugin_names, clifilter_plugin_list))
		plugin_change = true;
	else
		plugin_change = false;
	slurm_mutex_unlock(&g_context_lock);

	if (plugin_change) {
		info("CliFilterPlugins changed to %s", plugin_names);
		rc = cli_filter_plugin_fini();
		if (rc == SLURM_SUCCESS)
			rc = cli_filter_plugin_init();
	}
	xfree(plugin_names);

	return rc;
}

/*
 * Execute the pre_submit() function in each cli filter plugin.
 * If any plugin function returns anything other than SLURM_SUCCESS
 * then stop and forward it's return value.
 * IN cli_type - enumerated value from cli_types indicating the
 *               cli application in-use, use to interpret opt ptr
 * IN/OUT opt - pointer to {salloc|sbatch|srun}_opt_t data structure
 *              (value of pointer cannot change, but OK to mutate some
 *              of the values within the dereferenced memory)
 * OUT err_msg - Custom error message to the user, caller to xfree results
 */
extern int cli_filter_plugin_pre_submit(int type, void *data, char **err_msg) {
	DEF_TIMERS;
	int i, rc;

	START_TIMER;
	rc = cli_filter_plugin_init();
	slurm_mutex_lock(&g_context_lock);
	for (i = 0; ((i < g_context_cnt) && (rc == SLURM_SUCCESS)); i++)
		rc = (*(ops[i].pre_submit))(type, data, err_msg);
	slurm_mutex_unlock(&g_context_lock);
	END_TIMER2("cli_filter_plugin_pre_submit");

	return rc;
}

extern int cli_filter_plugin_post_submit(int type, uint32_t jobid, void *data, char **err_msg) {
	DEF_TIMERS;
	int i, rc;

	START_TIMER;
	rc = cli_filter_plugin_init();
	slurm_mutex_lock(&g_context_lock);
	for (i = 0; ((i < g_context_cnt) && (rc == SLURM_SUCCESS)); i++)
		rc = (*(ops[i].post_submit))(type, jobid, data, err_msg);
	slurm_mutex_unlock(&g_context_lock);
	END_TIMER2("cli_filter_plugin_post_submit");

	return rc;
}
