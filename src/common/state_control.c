/*****************************************************************************\
 *  state_control.c - state control common functions
 *****************************************************************************
 *  Copyright (C) 2017 SchedMD LLC.
 *  Written by Alejandro Sanchez <alex@schedmd.com>
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

#include "src/common/state_control.h"
#include "src/common/working_cluster.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

extern int _is_corecnt_supported(void)
{
	uint32_t select_type = slurmdb_setup_plugin_id_select();

	if (select_type != SELECT_PLUGIN_CONS_RES &&
	    select_type != SELECT_PLUGIN_CRAY_CONS_RES)
		return SLURM_ERROR;

	return SLURM_SUCCESS;
}

extern int _parse_resv_core_cnt(resv_desc_msg_t *resv_msg_ptr, char *val,
				int *free_tres_corecnt,	bool from_tres,
				char **err_msg)
{
	char *endptr = NULL, *core_cnt, *tok, *ptrptr = NULL;
	int node_inx = 0;

	/*
	 * CoreCnt and TRES=cpu= might appear within the same request,
	 * so we free the first and realloc the second.
	 */
	if (*free_tres_corecnt)
		xfree(resv_msg_ptr->core_cnt);

	core_cnt = xstrdup(val);
	tok = strtok_r(core_cnt, ",", &ptrptr);
	while (tok) {
		xrealloc(resv_msg_ptr->core_cnt,
			 sizeof(uint32_t) * (node_inx + 2));
		*free_tres_corecnt = 1;
		resv_msg_ptr->core_cnt[node_inx] =
			strtol(tok, &endptr, 10);
		if ((endptr == NULL) ||
		    (endptr[0] != '\0') ||
		    (tok[0] == '\0')) {
			if (err_msg) {
				if (from_tres)
					xstrfmtcat(*err_msg,
						   "Invalid TRES core count %s",
						   val);
				else
					xstrfmtcat(*err_msg,
						   "Invalid core count %s",
						   val);
			}
			xfree(core_cnt);
			return SLURM_ERROR;
		}
		node_inx++;
		tok = strtok_r(NULL, ",", &ptrptr);
	}

	xfree(core_cnt);
	return SLURM_SUCCESS;

}

extern int _parse_resv_node_cnt(resv_desc_msg_t *resv_msg_ptr, char *val,
				int *free_tres_nodecnt, bool from_tres,
				char **err_msg)
{
	char *endptr = NULL, *node_cnt, *tok, *ptrptr = NULL;
	int node_inx = 0;

	/*
	 * NodeCnt and TRES=node= might appear within the same request,
	 * so we free the first and realloc the second.
	 */
	if (*free_tres_nodecnt)
		xfree(resv_msg_ptr->node_cnt);

	node_cnt = xstrdup(val);
	tok = strtok_r(node_cnt, ",", &ptrptr);
	while (tok) {
		xrealloc(resv_msg_ptr->node_cnt,
			sizeof(uint32_t) * (node_inx + 2));
		*free_tres_nodecnt = 1;
		resv_msg_ptr->node_cnt[node_inx] =
			strtol(tok, &endptr, 10);
		if ((endptr != NULL) &&
		    ((endptr[0] == 'k') ||
		    (endptr[0] == 'K'))) {
			resv_msg_ptr->node_cnt[node_inx] *= 1024;
		} else if ((endptr != NULL) &&
			   ((endptr[0] == 'm') ||
			   (endptr[0] == 'M'))) {
			resv_msg_ptr->node_cnt[node_inx] *= 1024 * 1024;
		} else if ((endptr == NULL) ||
			   (endptr[0] != '\0') ||
			   (tok[0] == '\0')) {
			if (err_msg) {
				xfree(*err_msg);
				if (from_tres)
					xstrfmtcat(*err_msg,
						   "Invalid TRES node count %s",
						   val);
				else
					xstrfmtcat(*err_msg,
						   "Invalid node count %s",
						   val);
			}
			xfree(node_cnt);
			return SLURM_ERROR;
		}
		node_inx++;
		tok = strtok_r(NULL, ",", &ptrptr);
	}

	xfree(node_cnt);
	return SLURM_SUCCESS;
}

