/*****************************************************************************\
 *  cli_filter.h - driver for the cli_filter plugin
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

#ifndef _CLI_FILTER_H
#define _CLI_FILTER_H

#include "slurm/slurm.h"

enum cli_types {
	CLI_INVALID = 0,
	CLI_SALLOC = 1,
	CLI_SBATCH,
	CLI_SRUN,
	CLI_SBCAST,
	CLI_END
};
extern const char *cli_filter_labels[];

/*
 * Initialize the cli filter plugin.
 *
 * Returns a SLURM errno.
 */
extern int cli_filter_plugin_init(void);

/*
 * Terminate the cli filter plugin. Free memory.
 *
 * Returns a SLURM errno.
 */
extern int cli_filter_plugin_fini(void);

/*
 **************************************************************************
 *                          P L U G I N   C A L L S                       *
 **************************************************************************
 */

/*
 * Perform reconfig, re-read any configuration files
 */
extern int cli_filter_plugin_reconfig(void);

extern int cli_filter_plugin_setup_defaults(int cli_type, void *opt);

/*
 * Execute the pre_submit() function in each cli filter plugin.
 * If any plugin function returns anything other than SLURM_SUCCESS
 * then stop and forward it's return value.
 * IN cli_type - enumerated value from cli_types indicating the
 *               cli application in-use, use to interpret opt ptr
 * IN/OUT opt - pointer to {salloc|sbatch|srun}_opt_t data structure
 *              (value of pointer cannot change, but OK to mutate some
 *              of the values within the dereferenced memory)
 */
extern int cli_filter_plugin_pre_submit(int cli_type, void *opt);

extern int cli_filter_plugin_post_submit(int cli_type, uint32_t jobid, void *opt);

#endif /* !_CLI_FILTER_H */
