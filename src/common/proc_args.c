/*****************************************************************************\
 *  proc_args.c - helper functions for command argument processing
 *****************************************************************************
 *  Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 *  Portions Copyright (C) 2010-2015 SchedMD LLC <https://www.schedmd.com>.
 *  Written by Christopher Holmes <cholmes@hp.com>, who borrowed heavily
 *  from existing Slurm source code, particularly src/srun/opt.c
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
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
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include "config.h"

#ifndef __USE_ISOC99
#define __USE_ISOC99
#endif

#define _GNU_SOURCE

#ifndef SYSTEM_DIMENSIONS
#  define SYSTEM_DIMENSIONS 1
#endif

#include <ctype.h>		/* isdigit    */
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>		/* getpwuid   */
#include <stdarg.h>		/* va_start   */
#include <stdio.h>
#include <stdlib.h>		/* getenv, strtoll */
#include <string.h>		/* strcpy */
#include <sys/param.h>		/* MAXPATHLEN */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "src/common/gres.h"
#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/proc_args.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_acct_gather_profile.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"


/* print this version of Slurm */
void print_slurm_version(void)
{
	printf("%s %s\n", PACKAGE_NAME, SLURM_VERSION_STRING);
}

/* print the available gres options */
void print_gres_help(void)
{
	char *msg = gres_plugin_help_msg();
	printf("%s", msg);
	xfree(msg);
}

void set_distribution(task_dist_states_t distribution,
		      char **dist, char **lllp_dist)
{
	if (((int)distribution >= 0)
	    && ((distribution & SLURM_DIST_STATE_BASE) != SLURM_DIST_UNKNOWN)) {
		switch (distribution & SLURM_DIST_STATE_BASE) {
		case SLURM_DIST_CYCLIC:
			*dist      = "cyclic";
			break;
		case SLURM_DIST_BLOCK:
			*dist      = "block";
			break;
		case SLURM_DIST_PLANE:
			*dist      = "plane";
			*lllp_dist = "plane";
			break;
		case SLURM_DIST_ARBITRARY:
			*dist      = "arbitrary";
			break;
		case SLURM_DIST_CYCLIC_CYCLIC:
			*dist      = "cyclic:cyclic";
			*lllp_dist = "cyclic";
			break;
		case SLURM_DIST_CYCLIC_BLOCK:
			*dist      = "cyclic:block";
			*lllp_dist = "block";
			break;
		case SLURM_DIST_BLOCK_CYCLIC:
			*dist      = "block:cyclic";
			*lllp_dist = "cyclic";
			break;
		case SLURM_DIST_BLOCK_BLOCK:
			*dist      = "block:block";
			*lllp_dist = "block";
			break;
		case SLURM_DIST_CYCLIC_CFULL:
			*dist      = "cyclic:fcyclic";
			*lllp_dist = "fcyclic";
			break;
		case SLURM_DIST_BLOCK_CFULL:
			*dist      = "block:fcyclic";
			*lllp_dist = "cyclic";
			break;
		case SLURM_DIST_CYCLIC_CYCLIC_CYCLIC:
			*dist      = "cyclic:cyclic:cyclic";
			*lllp_dist = "cyclic:cyclic";
			break;
		case SLURM_DIST_CYCLIC_CYCLIC_BLOCK:
			*dist      = "cyclic:cyclic:block";
			*lllp_dist = "cyclic:block";
			break;
		case SLURM_DIST_CYCLIC_CYCLIC_CFULL:
			*dist      = "cyclic:cyclic:fcyclic";
			*lllp_dist = "cyclic:fcyclic";
			break;
		case SLURM_DIST_CYCLIC_BLOCK_CYCLIC:
			*dist      = "cyclic:block:cyclic";
			*lllp_dist = "block:cyclic";
			break;
		case SLURM_DIST_CYCLIC_BLOCK_BLOCK:
			*dist      = "cyclic:block:block";
			*lllp_dist = "block:block";
			break;
		case SLURM_DIST_CYCLIC_BLOCK_CFULL:
			*dist      = "cyclic:cylic:cyclic";
			*lllp_dist = "cyclic:cyclic";
			break;
		case SLURM_DIST_CYCLIC_CFULL_CYCLIC:
			*dist      = "cyclic:cylic:cyclic";
			*lllp_dist = "cyclic:cyclic";
			break;
		case SLURM_DIST_CYCLIC_CFULL_BLOCK:
			*dist      = "cyclic:fcyclic:block";
			*lllp_dist = "fcyclic:block";
			break;
		case SLURM_DIST_CYCLIC_CFULL_CFULL:
			*dist      = "cyclic:fcyclic:fcyclic";
			*lllp_dist = "fcyclic:fcyclic";
			break;
		case SLURM_DIST_BLOCK_CYCLIC_CYCLIC:
			*dist      = "block:cyclic:cyclic";
			*lllp_dist = "cyclic:cyclic";
			break;
		case SLURM_DIST_BLOCK_CYCLIC_BLOCK:
			*dist      = "block:cyclic:block";
			*lllp_dist = "cyclic:block";
			break;
		case SLURM_DIST_BLOCK_CYCLIC_CFULL:
			*dist      = "block:cyclic:fcyclic";
			*lllp_dist = "cyclic:fcyclic";
			break;
		case SLURM_DIST_BLOCK_BLOCK_CYCLIC:
			*dist      = "block:block:cyclic";
			*lllp_dist = "block:cyclic";
			break;
		case SLURM_DIST_BLOCK_BLOCK_BLOCK:
			*dist      = "block:block:block";
			*lllp_dist = "block:block";
			break;
		case SLURM_DIST_BLOCK_BLOCK_CFULL:
			*dist      = "block:block:fcyclic";
			*lllp_dist = "block:fcyclic";
			break;
		case SLURM_DIST_BLOCK_CFULL_CYCLIC:
			*dist      = "block:fcyclic:cyclic";
			*lllp_dist = "fcyclic:cyclic";
			break;
		case SLURM_DIST_BLOCK_CFULL_BLOCK:
			*dist      = "block:fcyclic:block";
			*lllp_dist = "fcyclic:block";
			break;
		case SLURM_DIST_BLOCK_CFULL_CFULL:
			*dist      = "block:fcyclic:fcyclic";
			*lllp_dist = "fcyclic:fcyclic";
			break;
		default:
			error("unknown dist, type 0x%X", distribution);
			break;
		}
	}
}

/*
 * verify that a distribution type in arg is of a known form
 * returns the task_dist_states, or -1 if state is unknown
 */
task_dist_states_t verify_dist_type(const char *arg, uint32_t *plane_size)
{
	int len;
	char *dist_str = NULL;
	task_dist_states_t result = SLURM_DIST_UNKNOWN;
	bool pack_nodes = false, no_pack_nodes = false;
	char *tok, *tmp, *save_ptr = NULL;
	int i, j;
	char *cur_ptr;
	char buf[3][25];
	buf[0][0] = '\0';
	buf[1][0] = '\0';
	buf[2][0] = '\0';
	char outstr[100];
	outstr[0]='\0';

	if (!arg)
		return result;

	tmp = xstrdup(arg);
	tok = strtok_r(tmp, ",", &save_ptr);
	while (tok) {
		bool lllp_dist = false, plane_dist = false;
		len = strlen(tok);
		dist_str = strchr(tok, ':');
		if (dist_str != NULL) {
			/* -m cyclic|block:cyclic|block */
			lllp_dist = true;
		} else {
			/* -m plane=<plane_size> */
			dist_str = strchr(tok, '=');
			if (!dist_str)
				dist_str = getenv("SLURM_DIST_PLANESIZE");
			else {
				len = dist_str - tok;
				dist_str++;
			}
			if (dist_str) {
				*plane_size = atoi(dist_str);
				plane_dist = true;
			}
		}

		cur_ptr = tok;
	 	for (j = 0; j < 3; j++) {
			for (i = 0; i < 24; i++) {
				if (*cur_ptr == '\0' || *cur_ptr ==':')
					break;
				buf[j][i] = *cur_ptr++;
			}
			buf[j][i] = '\0';
			if (*cur_ptr == '\0')
				break;
			buf[j][i] = '\0';
			cur_ptr++;
		}
		if (xstrcmp(buf[0], "*") == 0)
			/* default node distribution is block */
			strcpy(buf[0], "block");
		strcat(outstr, buf[0]);
		if (xstrcmp(buf[1], "\0") != 0) {
			strcat(outstr, ":");
			if (!xstrcmp(buf[1], "*") || !xstrcmp(buf[1], "\0")) {
				/* default socket distribution is cyclic */
				strcpy(buf[1], "cyclic");
			}
			strcat(outstr, buf[1]);
		}
		if (xstrcmp(buf[2], "\0") != 0) {
			strcat(outstr, ":");
			if (!xstrcmp(buf[2], "*") || !xstrcmp(buf[2], "\0")) {
				/* default core dist is inherited socket dist */
				strcpy(buf[2], buf[1]);
			}
			strcat(outstr, buf[2]);
		}

		if (lllp_dist) {
			if (xstrcasecmp(outstr, "cyclic:cyclic") == 0) {
				result = SLURM_DIST_CYCLIC_CYCLIC;
			} else if (xstrcasecmp(outstr, "cyclic:block") == 0) {
				result = SLURM_DIST_CYCLIC_BLOCK;
			} else if (xstrcasecmp(outstr, "block:block") == 0) {
				result = SLURM_DIST_BLOCK_BLOCK;
			} else if (xstrcasecmp(outstr, "block:cyclic") == 0) {
				result = SLURM_DIST_BLOCK_CYCLIC;
			} else if (xstrcasecmp(outstr, "block:fcyclic") == 0) {
				result = SLURM_DIST_BLOCK_CFULL;
			} else if (xstrcasecmp(outstr, "cyclic:fcyclic") == 0) {
				result = SLURM_DIST_CYCLIC_CFULL;
			} else if (xstrcasecmp(outstr, "cyclic:cyclic:cyclic")
				   == 0) {
				result = SLURM_DIST_CYCLIC_CYCLIC_CYCLIC;
			} else if (xstrcasecmp(outstr, "cyclic:cyclic:block")
				   == 0) {
				result = SLURM_DIST_CYCLIC_CYCLIC_BLOCK;
			} else if (xstrcasecmp(outstr, "cyclic:cyclic:fcyclic")
				== 0) {
				result = SLURM_DIST_CYCLIC_CYCLIC_CFULL;
			} else if (xstrcasecmp(outstr, "cyclic:block:cyclic")
				== 0) {
				result = SLURM_DIST_CYCLIC_BLOCK_CYCLIC;
			} else if (xstrcasecmp(outstr, "cyclic:block:block")
				== 0) {
				result = SLURM_DIST_CYCLIC_BLOCK_BLOCK;
			} else if (xstrcasecmp(outstr, "cyclic:block:fcyclic")
				== 0) {
				result = SLURM_DIST_CYCLIC_BLOCK_CFULL;
			} else if (xstrcasecmp(outstr, "cyclic:fcyclic:cyclic")
				== 0) {
				result = SLURM_DIST_CYCLIC_CFULL_CYCLIC;
			} else if (xstrcasecmp(outstr, "cyclic:fcyclic:block")
				== 0) {
				result = SLURM_DIST_CYCLIC_CFULL_BLOCK;
			} else if (xstrcasecmp(outstr, "cyclic:fcyclic:fcyclic")
				== 0) {
				result = SLURM_DIST_CYCLIC_CFULL_CFULL;
			} else if (xstrcasecmp(outstr, "block:cyclic:cyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_CYCLIC_CYCLIC;
			} else if (xstrcasecmp(outstr, "block:cyclic:block")
				== 0) {
				result = SLURM_DIST_BLOCK_CYCLIC_BLOCK;
			} else if (xstrcasecmp(outstr, "block:cyclic:fcyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_CYCLIC_CFULL;
			} else if (xstrcasecmp(outstr, "block:block:cyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_BLOCK_CYCLIC;
			} else if (xstrcasecmp(outstr, "block:block:block")
				== 0) {
				result = SLURM_DIST_BLOCK_BLOCK_BLOCK;
			} else if (xstrcasecmp(outstr, "block:block:fcyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_BLOCK_CFULL;
			} else if (xstrcasecmp(outstr, "block:fcyclic:cyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_CFULL_CYCLIC;
			} else if (xstrcasecmp(outstr, "block:fcyclic:block")
				== 0) {
				result = SLURM_DIST_BLOCK_CFULL_BLOCK;
			} else if (xstrcasecmp(outstr, "block:fcyclic:fcyclic")
				== 0) {
				result = SLURM_DIST_BLOCK_CFULL_CFULL;
			}
		} else if (plane_dist) {
			if (xstrncasecmp(tok, "plane", len) == 0) {
				result = SLURM_DIST_PLANE;
			}
		} else {
			if (xstrncasecmp(tok, "cyclic", len) == 0) {
				result = SLURM_DIST_CYCLIC;
			} else if (xstrncasecmp(tok, "block", len) == 0) {
				result = SLURM_DIST_BLOCK;
			} else if ((xstrncasecmp(tok, "arbitrary", len) == 0) ||
				   (xstrncasecmp(tok, "hostfile", len) == 0)) {
				result = SLURM_DIST_ARBITRARY;
			} else if (xstrncasecmp(tok, "nopack", len) == 0) {
				no_pack_nodes = true;
			} else if (xstrncasecmp(tok, "pack", len) == 0) {
				pack_nodes = true;
			}
		}
		tok = strtok_r(NULL, ",", &save_ptr);
	}
	xfree(tmp);

	if (pack_nodes)
		result |= SLURM_DIST_PACK_NODES;
	else if (no_pack_nodes)
		result |= SLURM_DIST_NO_PACK_NODES;

	return result;
}

extern char *format_task_dist_states(task_dist_states_t t)
{
	switch (t & SLURM_DIST_STATE_BASE) {
	case SLURM_DIST_BLOCK:
		return "block";
	case SLURM_DIST_CYCLIC:
		return "cyclic";
	case SLURM_DIST_PLANE:
		return "plane";
	case SLURM_DIST_ARBITRARY:
		return "arbitrary";
	case SLURM_DIST_CYCLIC_CYCLIC:
		return "cyclic:cyclic";
	case SLURM_DIST_CYCLIC_BLOCK:
		return "cyclic:block";
	case SLURM_DIST_CYCLIC_CFULL:
		return "cyclic:fcyclic";
	case SLURM_DIST_BLOCK_CYCLIC:
		return "block:cyclic";
	case SLURM_DIST_BLOCK_BLOCK:
		return "block:block";
	case SLURM_DIST_BLOCK_CFULL:
		return "block:fcyclic";
	case SLURM_DIST_CYCLIC_CYCLIC_CYCLIC:
		return "cyclic:cyclic:cyclic";
	case SLURM_DIST_CYCLIC_CYCLIC_BLOCK:
		return "cyclic:cyclic:block";
	case SLURM_DIST_CYCLIC_CYCLIC_CFULL:
		return "cyclic:cyclic:fcyclic";
	case SLURM_DIST_CYCLIC_BLOCK_CYCLIC:
		return "cyclic:block:cyclic";
	case SLURM_DIST_CYCLIC_BLOCK_BLOCK:
		return "cyclic:block:block";
	case SLURM_DIST_CYCLIC_BLOCK_CFULL:
		return "cyclic:block:fcyclic";
	case SLURM_DIST_CYCLIC_CFULL_CYCLIC:
		return "cyclic:fcyclic:cyclic" ;
	case SLURM_DIST_CYCLIC_CFULL_BLOCK:
		return "cyclic:fcyclic:block";
	case SLURM_DIST_CYCLIC_CFULL_CFULL:
		return "cyclic:fcyclic:fcyclic";
	case SLURM_DIST_BLOCK_CYCLIC_CYCLIC:
		return "block:cyclic:cyclic";
	case SLURM_DIST_BLOCK_CYCLIC_BLOCK:
		return "block:cyclic:block";
	case SLURM_DIST_BLOCK_CYCLIC_CFULL:
		return "block:cyclic:fcyclic";
	case SLURM_DIST_BLOCK_BLOCK_CYCLIC:
		return "block:block:cyclic";
	case SLURM_DIST_BLOCK_BLOCK_BLOCK:
		return "block:block:block";
	case SLURM_DIST_BLOCK_BLOCK_CFULL:
		return "block:block:fcyclic";
	case SLURM_DIST_BLOCK_CFULL_CYCLIC:
		return "block:fcyclic:cyclic";
	case SLURM_DIST_BLOCK_CFULL_BLOCK:
		return "block:fcyclic:block";
	case SLURM_DIST_BLOCK_CFULL_CFULL:
		return "block:fcyclic:fcyclic";
	default:
		return "unknown";
	}
}

/* return command name from its full path name */
char *base_name(const char *command)
{
	const char *char_ptr;

	if (command == NULL)
		return NULL;

	char_ptr = strrchr(command, (int)'/');
	if (char_ptr == NULL)
		char_ptr = command;
	else
		char_ptr++;

	return xstrdup(char_ptr);
}

static long _str_to_mbytes(const char *arg, int use_gbytes)
{
	long result;
	char *endptr;

	errno = 0;
	result = strtol(arg, &endptr, 10);
	if ((errno != 0) && ((result == LONG_MIN) || (result == LONG_MAX)))
		result = -1;
	else if ((endptr[0] == '\0') && (use_gbytes == 1))  /* GB default */
		result *= 1024;
	else if (endptr[0] == '\0')	/* MB default */
		;
	else if ((endptr[0] == 'k') || (endptr[0] == 'K'))
		result = (result + 1023) / 1024;	/* round up */
	else if ((endptr[0] == 'm') || (endptr[0] == 'M'))
		;
	else if ((endptr[0] == 'g') || (endptr[0] == 'G'))
		result *= 1024;
	else if ((endptr[0] == 't') || (endptr[0] == 'T'))
		result *= (1024 * 1024);
	else
		result = -1;

	return result;
}

/*
 * str_to_mbytes(): verify that arg is numeric with optional "K", "M", "G"
 * or "T" at end and return the number in mega-bytes. Default units are MB.
 */
long str_to_mbytes(const char *arg)
{
	return _str_to_mbytes(arg, 0);
}

/*
 * str_to_mbytes2(): verify that arg is numeric with optional "K", "M", "G"
 * or "T" at end and return the number in mega-bytes. Default units are GB
 * if "SchedulerParameters=default_gbytes" is configured, otherwise MB.
 */
long str_to_mbytes2(const char *arg)
{
	static int use_gbytes = -1;

	if (use_gbytes == -1) {
		char *sched_params = slurm_get_sched_params();
		if (sched_params && xstrcasestr(sched_params, "default_gbytes"))
			use_gbytes = 1;
		else
			use_gbytes = 0;
		xfree(sched_params);
	}

	return _str_to_mbytes(arg, use_gbytes);
}

/* Convert a string into a node count */
static int
_str_to_nodes(const char *num_str, char **leftover)
{
	long int num;
	char *endptr;

	num = strtol(num_str, &endptr, 10);
	if (endptr == num_str) { /* no valid digits */
		*leftover = (char *)num_str;
		return -1;
	}
	if (*endptr != '\0' && (*endptr == 'k' || *endptr == 'K')) {
		num *= 1024;
		endptr++;
	}
	if (*endptr != '\0' && (*endptr == 'm' || *endptr == 'M')) {
		num *= (1024 * 1024);
		endptr++;
	}
	*leftover = endptr;

	return (int)num;
}

/*
 * verify that a node count in arg is of a known form (count or min-max)
 * OUT min, max specified minimum and maximum node counts
 * RET true if valid
 */
bool verify_node_count(const char *arg, int *min_nodes, int *max_nodes)
{
	char *ptr, *min_str, *max_str;
	char *leftover;

	/*
	 * Does the string contain a "-" character?  If so, treat as a range.
	 * otherwise treat as an absolute node count.
	 */
	if ((ptr = xstrchr(arg, '-')) != NULL) {
		min_str = xstrndup(arg, ptr-arg);
		*min_nodes = _str_to_nodes(min_str, &leftover);
		if (!xstring_is_whitespace(leftover)) {
			error("\"%s\" is not a valid node count", min_str);
			xfree(min_str);
			return false;
		}
		xfree(min_str);
		if (*min_nodes < 0)
			*min_nodes = 1;

		max_str = xstrndup(ptr+1, strlen(arg)-((ptr+1)-arg));
		*max_nodes = _str_to_nodes(max_str, &leftover);
		if (!xstring_is_whitespace(leftover)) {
			error("\"%s\" is not a valid node count", max_str);
			xfree(max_str);
			return false;
		}
		xfree(max_str);
	} else {
		*min_nodes = *max_nodes = _str_to_nodes(arg, &leftover);
		if (!xstring_is_whitespace(leftover)) {
			error("\"%s\" is not a valid node count", arg);
			return false;
		}
		if (*min_nodes < 0) {
			error("\"%s\" is not a valid node count", arg);
			return false;
		}
	}

	if ((*max_nodes != 0) && (*max_nodes < *min_nodes)) {
		error("Maximum node count %d is less than minimum node count %d",
		      *max_nodes, *min_nodes);
		return false;
	}

	return true;
}

/*
 * If the node list supplied is a file name, translate that into
 *	a list of nodes, we orphan the data pointed to
 * RET true if the node list is a valid one
 */
bool verify_node_list(char **node_list_pptr, enum task_dist_states dist,
		      int task_count)
{
	char *nodelist = NULL;

	xassert (node_list_pptr);
	xassert (*node_list_pptr);

	if (strchr(*node_list_pptr, '/') == NULL)
		return true;	/* not a file name */

	/* If we are using Arbitrary grab count out of the hostfile
	   using them exactly the way we read it in since we are
	   saying, lay it out this way! */
	if ((dist & SLURM_DIST_STATE_BASE) == SLURM_DIST_ARBITRARY)
		nodelist = slurm_read_hostfile(*node_list_pptr, task_count);
	else
		nodelist = slurm_read_hostfile(*node_list_pptr, NO_VAL);

	if (!nodelist)
		return false;

	xfree(*node_list_pptr);
	*node_list_pptr = xstrdup(nodelist);
	free(nodelist);

	return true;
}

/*
 * get either 1 or 2 integers for a resource count in the form of either
 * (count, min-max, or '*')
 * A partial error message is passed in via the 'what' param.
 * IN arg - argument
 * IN what - variable name (for errors)
 * OUT min - first number
 * OUT max - maximum value if specified, NULL if don't care
 * IN isFatal - if set, exit on error
 * RET true if valid
 */
bool get_resource_arg_range(const char *arg, const char *what, int* min,
			    int *max, bool isFatal)
{
	char *p;
	long int result;

	/* wildcard meaning every possible value in range */
	if ((*arg == '\0') || (*arg == '*' )) {
		*min = 1;
		if (max)
			*max = INT_MAX;
		return true;
	}

	result = strtol(arg, &p, 10);
	if (*p == 'k' || *p == 'K') {
		result *= 1024;
		p++;
	} else if (*p == 'm' || *p == 'M') {
		result *= 1048576;
		p++;
	}

	if (((*p != '\0') && (*p != '-')) || (result < 0L)) {
		error ("Invalid numeric value \"%s\" for %s.", arg, what);
		if (isFatal)
			exit(1);
		return false;
	} else if (result > INT_MAX) {
		error ("Numeric argument (%ld) to big for %s.", result, what);
		if (isFatal)
			exit(1);
		return false;
	}

	*min = (int) result;

	if (*p == '\0')
		return true;
	if (*p == '-')
		p++;

	result = strtol(p, &p, 10);
	if ((*p == 'k') || (*p == 'K')) {
		result *= 1024;
		p++;
	} else if (*p == 'm' || *p == 'M') {
		result *= 1048576;
		p++;
	}

	if (((*p != '\0') && (*p != '-')) || (result <= 0L)) {
		error ("Invalid numeric value \"%s\" for %s.", arg, what);
		if (isFatal)
			exit(1);
		return false;
	} else if (result > INT_MAX) {
		error ("Numeric argument (%ld) to big for %s.", result, what);
		if (isFatal)
			exit(1);
		return false;
	}

	if (max)
		*max = (int) result;

	return true;
}

/*
 * verify that a resource counts in arg are of a known form X, X:X, X:X:X, or
 * X:X:X:X, where X is defined as either (count, min-max, or '*')
 * RET true if valid
 */
bool verify_socket_core_thread_count(const char *arg, int *min_sockets,
				     int *min_cores, int *min_threads,
				     cpu_bind_type_t *cpu_bind_type)
{
	bool tmp_val, ret_val;
	int i, j;
	int max_sockets = 0, max_cores = 0, max_threads = 0;
	const char *cur_ptr = arg;
	char buf[3][48]; /* each can hold INT64_MAX - INT64_MAX */

	if (!arg) {
		error("%s: argument is NULL", __func__);
		return false;
	}
	memset(buf, 0, sizeof(buf));
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 47; i++) {
			if (*cur_ptr == '\0' || *cur_ptr ==':')
				break;
			buf[j][i] = *cur_ptr++;
		}
		if (*cur_ptr == '\0')
			break;
		xassert(*cur_ptr == ':');
		cur_ptr++;
	}
	/* if cpu_bind_type doesn't already have a auto preference, choose
	 * the level based on the level of the -E specification
	 */
	if (cpu_bind_type &&
	    !(*cpu_bind_type & (CPU_BIND_TO_SOCKETS |
				CPU_BIND_TO_CORES |
				CPU_BIND_TO_THREADS))) {
		if (j == 0) {
			*cpu_bind_type |= CPU_BIND_TO_SOCKETS;
		} else if (j == 1) {
			*cpu_bind_type |= CPU_BIND_TO_CORES;
		} else if (j == 2) {
			*cpu_bind_type |= CPU_BIND_TO_THREADS;
		}
	}

	ret_val = true;
	tmp_val = get_resource_arg_range(&buf[0][0], "first arg of -B",
					 min_sockets, &max_sockets, true);
	if ((*min_sockets == 1) && (max_sockets == INT_MAX))
		*min_sockets = NO_VAL;	/* Use full range of values */
	ret_val = ret_val && tmp_val;


	tmp_val = get_resource_arg_range(&buf[1][0], "second arg of -B",
					 min_cores, &max_cores, true);
	if ((*min_cores == 1) && (max_cores == INT_MAX))
		*min_cores = NO_VAL;	/* Use full range of values */
	ret_val = ret_val && tmp_val;

	tmp_val = get_resource_arg_range(&buf[2][0], "third arg of -B",
					 min_threads, &max_threads, true);
	if ((*min_threads == 1) && (max_threads == INT_MAX))
		*min_threads = NO_VAL;	/* Use full range of values */
	ret_val = ret_val && tmp_val;

	return ret_val;
}

/*
 * verify that a hint is valid and convert it into the implied settings
 * RET true if valid
 */
bool verify_hint(const char *arg, int *min_sockets, int *min_cores,
		 int *min_threads, int *ntasks_per_core,
		 cpu_bind_type_t *cpu_bind_type)
{
	char *buf, *p, *tok;

	if (!arg)
		return true;

	buf = xstrdup(arg);
	p = buf;
	/* change all ',' delimiters not followed by a digit to ';'  */
	/* simplifies parsing tokens while keeping map/mask together */
	while (p[0] != '\0') {
		if ((p[0] == ',') && (!isdigit((int)p[1])))
			p[0] = ';';
		p++;
	}

	p = buf;
	while ((tok = strsep(&p, ";"))) {
		if (xstrcasecmp(tok, "help") == 0) {
			printf(
"Application hint options:\n"
"    --hint=             Bind tasks according to application hints\n"
"        compute_bound   use all cores in each socket\n"
"        memory_bound    use only one core in each socket\n"
"        [no]multithread [don't] use extra threads with in-core multi-threading\n"
"        help            show this help message\n");
			xfree(buf);
			return 1;
		} else if (xstrcasecmp(tok, "compute_bound") == 0) {
			*min_sockets = NO_VAL;
			*min_cores   = NO_VAL;
			*min_threads = 1;
			if (cpu_bind_type)
				*cpu_bind_type |= CPU_BIND_TO_CORES;
		} else if (xstrcasecmp(tok, "memory_bound") == 0) {
			*min_cores   = 1;
			*min_threads = 1;
			if (cpu_bind_type)
				*cpu_bind_type |= CPU_BIND_TO_CORES;
		} else if (xstrcasecmp(tok, "multithread") == 0) {
			*min_threads = NO_VAL;
			if (cpu_bind_type) {
				*cpu_bind_type |= CPU_BIND_TO_THREADS;
				*cpu_bind_type &=
					(~CPU_BIND_ONE_THREAD_PER_CORE);
			}
			if (*ntasks_per_core == NO_VAL)
				*ntasks_per_core = INFINITE;
		} else if (xstrcasecmp(tok, "nomultithread") == 0) {
			*min_threads = 1;
			if (cpu_bind_type) {
				*cpu_bind_type |= CPU_BIND_TO_THREADS;
				*cpu_bind_type |= CPU_BIND_ONE_THREAD_PER_CORE;
			}
		} else {
			error("unrecognized --hint argument \"%s\", "
			      "see --hint=help", tok);
			xfree(buf);
			return 1;
		}
	}

	if (!cpu_bind_type)
		setenvf(NULL, "SLURM_HINT", "%s", arg);

	xfree(buf);
	return 0;
}

uint16_t parse_mail_type(const char *arg)
{
	char *buf, *tok, *save_ptr = NULL;
	uint16_t rc = 0;
	bool none_set = false;

	if (!arg)
		return INFINITE16;

	buf = xstrdup(arg);
	tok = strtok_r(buf, ",", &save_ptr);
	while (tok) {
		if (xstrcasecmp(tok, "NONE") == 0) {
			rc = 0;
			none_set = true;
			break;
		}
		else if (xstrcasecmp(tok, "ARRAY_TASKS") == 0)
			rc |= MAIL_ARRAY_TASKS;
		else if (xstrcasecmp(tok, "BEGIN") == 0)
			rc |= MAIL_JOB_BEGIN;
		else if  (xstrcasecmp(tok, "END") == 0)
			rc |= MAIL_JOB_END;
		else if (xstrcasecmp(tok, "FAIL") == 0)
			rc |= MAIL_JOB_FAIL;
		else if (xstrcasecmp(tok, "REQUEUE") == 0)
			rc |= MAIL_JOB_REQUEUE;
		else if (xstrcasecmp(tok, "ALL") == 0)
			rc |= MAIL_JOB_BEGIN |  MAIL_JOB_END |  MAIL_JOB_FAIL |
			      MAIL_JOB_REQUEUE | MAIL_JOB_STAGE_OUT;
		else if (!xstrcasecmp(tok, "STAGE_OUT"))
			rc |= MAIL_JOB_STAGE_OUT;
		else if (xstrcasecmp(tok, "TIME_LIMIT") == 0)
			rc |= MAIL_JOB_TIME100;
		else if (xstrcasecmp(tok, "TIME_LIMIT_90") == 0)
			rc |= MAIL_JOB_TIME90;
		else if (xstrcasecmp(tok, "TIME_LIMIT_80") == 0)
			rc |= MAIL_JOB_TIME80;
		else if (xstrcasecmp(tok, "TIME_LIMIT_50") == 0)
			rc |= MAIL_JOB_TIME50;
		tok = strtok_r(NULL, ",", &save_ptr);
	}
	xfree(buf);
	if (!rc && !none_set)
		rc = INFINITE16;

	return rc;
}
char *print_mail_type(const uint16_t type)
{
	static char buf[256];

	buf[0] = '\0';

	if (type == 0)
		return "NONE";

	if (type & MAIL_ARRAY_TASKS) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "ARRAY_TASKS");
	}
	if (type & MAIL_JOB_BEGIN) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "BEGIN");
	}
	if (type & MAIL_JOB_END) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "END");
	}
	if (type & MAIL_JOB_FAIL) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "FAIL");
	}
	if (type & MAIL_JOB_REQUEUE) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "REQUEUE");
	}
	if (type & MAIL_JOB_STAGE_OUT) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "STAGE_OUT");
	}
	if (type & MAIL_JOB_TIME50) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "TIME_LIMIT_50");
	}
	if (type & MAIL_JOB_TIME80) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "TIME_LIMIT_80");
	}
	if (type & MAIL_JOB_TIME90) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "TIME_LIMIT_90");
	}
	if (type & MAIL_JOB_TIME100) {
		if (buf[0])
			strcat(buf, ",");
		strcat(buf, "TIME_LIMIT");
	}

	return buf;
}

static void
_freeF(void *data)
{
	xfree(data);
}

static List
_create_path_list(void)
{
	List l = list_create(_freeF);
	char *path;
	char *c, *lc;

	c = getenv("PATH");
	if (!c) {
		error("No PATH environment variable");
		return l;
	}
	path = xstrdup(c);
	c = lc = path;

	while (*c != '\0') {
		if (*c == ':') {
			/* nullify and push token onto list */
			*c = '\0';
			if (lc != NULL && strlen(lc) > 0)
				list_append(l, xstrdup(lc));
			lc = ++c;
		} else
			c++;
	}

	if (strlen(lc) > 0)
		list_append(l, xstrdup(lc));

	xfree(path);

	return l;
}

/*
 * Check a specific path to see if the executable exists and is not a directory
 * IN path - path of executable to check
 * RET true if path exists and is not a directory; false otherwise
 */
static bool _exists(const char *path)
{
	struct stat st;
        if (stat(path, &st)) {
		debug2("_check_exec: failed to stat path %s", path);
		return false;
	}
	if (S_ISDIR(st.st_mode)) {
		debug2("_check_exec: path %s is a directory", path);
		return false;
	}
	return true;
}

/*
 * Check a specific path to see if the executable is accessible
 * IN path - path of executable to check
 * IN access_mode - determine if executable is accessible to caller with
 *		    specified mode
 * RET true if path is accessible according to access mode, false otherwise
 */
static bool _accessible(const char *path, int access_mode)
{
	if (access(path, access_mode)) {
		debug2("_check_exec: path %s is not accessible", path);
		return false;
	}
	return true;
}

/*
 * search PATH to confirm the location and access mode of the given command
 * IN cwd - current working directory
 * IN cmd - command to execute
 * IN check_current_dir - if true, search cwd for the command
 * IN access_mode - required access rights of cmd
 * IN test_exec - if false, do not confirm access mode of cmd if full path
 * RET full path of cmd or NULL if not found
 */
char *search_path(char *cwd, char *cmd, bool check_current_dir, int access_mode,
		  bool test_exec)
{
	List         l        = NULL;
	ListIterator i        = NULL;
	char *path, *fullpath = NULL;

	/* Relative path */
	if (cmd[0] == '.') {
		if (test_exec) {
			char *cmd1 = xstrdup_printf("%s/%s", cwd, cmd);
			if (_exists(cmd1) && _accessible(cmd1, access_mode))
				fullpath = xstrdup(cmd1);
			xfree(cmd1);
		}
		return fullpath;
	}
	/* Absolute path */
	if (cmd[0] == '/') {
		if (test_exec && _exists(cmd) && _accessible(cmd, access_mode))
			fullpath = xstrdup(cmd);
		return fullpath;
	}
	/* Otherwise search in PATH */
	l = _create_path_list();
	if (l == NULL)
		return NULL;

	/* Check cwd last, so local binaries do not trump binaries in PATH */
	if (check_current_dir)
		list_append(l, xstrdup(cwd));

	i = list_iterator_create(l);
	while ((path = list_next(i))) {
		if (path[0] == '.')
			xstrfmtcat(fullpath, "%s/%s/%s", cwd, path, cmd);
		else
			xstrfmtcat(fullpath, "%s/%s", path, cmd);
		/* Use first executable found in PATH */
		if (_exists(fullpath)) {
			if (!test_exec)
				break;
			if (_accessible(path, access_mode))
				break;
		}
		xfree(fullpath);
	}
	list_iterator_destroy(i);
	FREE_NULL_LIST(l);
	return fullpath;
}

char *print_commandline(const int script_argc, char **script_argv)
{
	int i;
	char *out_buf = NULL, *prefix = "";

	for (i = 0; i < script_argc; i++) {
		xstrfmtcat(out_buf,  "%s%s", prefix, script_argv[i]);
		prefix = " ";
	}
	return out_buf;
}

/* Translate a signal option string "--signal=<int>[@<time>]" into
 * it's warn_signal and warn_time components.
 * RET 0 on success, -1 on failure */
int get_signal_opts(char *optarg, uint16_t *warn_signal, uint16_t *warn_time,
		    uint16_t *warn_flags)
{
	char *endptr;
	long num;

	if (optarg == NULL)
		return -1;

	if (!xstrncasecmp(optarg, "B:", 2)) {
		*warn_flags = KILL_JOB_BATCH;
		optarg += 2;
	}

	endptr = strchr(optarg, '@');
	if (endptr)
		endptr[0] = '\0';
	num = (uint16_t) sig_name2num(optarg);
	if (endptr)
		endptr[0] = '@';
	if ((num < 1) || (num > 0x0ffff))
		return -1;
	*warn_signal = (uint16_t) num;

	if (!endptr) {
		*warn_time = 60;
		return 0;
	}

	num = strtol(endptr+1, &endptr, 10);
	if ((num < 0) || (num > 0x0ffff))
		return -1;
	*warn_time = (uint16_t) num;
	if (endptr[0] == '\0')
		return 0;
	return -1;
}

/* Convert a signal name to it's numeric equivalent.
 * Return 0 on failure */
int sig_name2num(char *signal_name)
{
	struct signal_name_value {
		char *name;
		uint16_t val;
	} signals[] = {
		{ "HUP",	SIGHUP	},
		{ "INT",	SIGINT	},
		{ "QUIT",	SIGQUIT	},
		{ "ABRT",	SIGABRT	},
		{ "KILL",	SIGKILL	},
		{ "ALRM",	SIGALRM	},
		{ "TERM",	SIGTERM	},
		{ "USR1",	SIGUSR1	},
		{ "USR2",	SIGUSR2	},
		{ "URG",	SIGURG	},
		{ "CONT",	SIGCONT	},
		{ "STOP",	SIGSTOP	},
		{ "TSTP",	SIGTSTP	},
		{ "TTIN",	SIGTTIN	},
		{ "TTOU",	SIGTTOU	},
		{ NULL,		0	}	/* terminate array */
	};
	char *ptr;
	long tmp;
	int i;

	tmp = strtol(signal_name, &ptr, 10);
	if (ptr != signal_name) { /* found a number */
		if (xstring_is_whitespace(ptr))
			return (int)tmp;
		else
			return 0;
	}

	/* search the array */
	ptr = signal_name;
	while (isspace((int)*ptr))
		ptr++;
	if (xstrncasecmp(ptr, "SIG", 3) == 0)
		ptr += 3;
	for (i = 0; ; i++) {
		int siglen;
		if (signals[i].name == NULL)
			return 0;
		siglen = strlen(signals[i].name);
		if ((!xstrncasecmp(ptr, signals[i].name, siglen)
		    && xstring_is_whitespace(ptr + siglen))) {
			/* found the signal name */
			return signals[i].val;
		}
	}

	return 0;	/* not found */
}

/*
 * parse_uint16 - Convert ascii string to a 16 bit unsigned int.
 * IN      aval - ascii string.
 * IN/OUT  ival - 16 bit pointer.
 * RET     0 if no error, 1 otherwise.
 */
extern int parse_uint16(char *aval, uint16_t *ival)
{
	/*
	 * First,  convert the ascii value it to a
	 * long long int. If the result is greater then
	 * or equal to 0 and less than NO_VAL16
	 * set the value and return. Otherwise
	 * return an error.
	 */
	uint16_t max16uint = NO_VAL16;
	long long tval;
	char *p;

	/*
	 * Return error for invalid value.
	 */
	tval = strtoll(aval, &p, 10);
	if (p[0] || (tval == LLONG_MIN) || (tval == LLONG_MAX) ||
	    (tval < 0) || (tval >= max16uint))
		return 1;

	*ival = (uint16_t) tval;

	return 0;
}

/*
 * parse_uint32 - Convert ascii string to a 32 bit unsigned int.
 * IN      aval - ascii string.
 * IN/OUT  ival - 32 bit pointer.
 * RET     0 if no error, 1 otherwise.
 */
extern int parse_uint32(char *aval, uint32_t *ival)
{
	/*
	 * First,  convert the ascii value it to a
	 * long long int. If the result is greater
	 * than or equal to 0 and less than NO_VAL
	 * set the value and return. Otherwise return
	 * an error.
	 */
	uint32_t max32uint = NO_VAL;
	long long tval;
	char *p;

	/*
	 * Return error for invalid value.
	 */
	tval = strtoll(aval, &p, 10);
	if (p[0] || (tval == LLONG_MIN) || (tval == LLONG_MAX) ||
	    (tval < 0) || (tval >= max32uint))
		return 1;

	*ival = (uint32_t) tval;

	return 0;
}

/*
 * parse_uint64 - Convert ascii string to a 64 bit unsigned int.
 * IN      aval - ascii string.
 * IN/OUT  ival - 64 bit pointer.
 * RET     0 if no error, 1 otherwise.
 */
extern int parse_uint64(char *aval, uint64_t *ival)
{
	/*
	 * First,  convert the ascii value it to an
	 * unsigned long long. If the result is greater
	 * than or equal to 0 and less than NO_VAL
	 * set the value and return. Otherwise return
	 * an error.
	 */
	uint64_t max64uint = NO_VAL64;
	long long tval;
	char *p;

	/*
 	 * Return error for invalid value.
	 */
	tval = strtoll(aval, &p, 10);
	if (p[0] || (tval == LLONG_MIN) || (tval == LLONG_MAX) ||
	    (tval < 0) || (tval >= max64uint))
		return 1;

	*ival = (uint64_t) tval;

	return 0;
}

/* A boolean env variable is true if:
 *  - set, but no argument
 *  - argument is "yes"
 *  - argument is a non-zero number
 */
extern bool parse_bool(const char *val) {
	char *end = NULL;
	if (!val)
		return false;
	if (val[0] == '\0')
		return true;
	if (xstrcasecmp(val, "yes") == 0)
		return true;
	if ((strtol(val, &end, 10) != 0) && end != val)
		return true;
	return false;
}

/*
 *  Get a decimal integer from arg.
 *
 *  Returns the integer on success, exits program on failure.
 */
extern int parse_int(const char *name, const char *val, bool positive)
{
	char *p = NULL;
	long int result = 0;

	if (val)
		result = strtol(val, &p, 10);

	if ((p == NULL) || (p[0] != '\0') || (result < 0L) ||
	    (positive && (result <= 0L))) {
		error ("Invalid numeric value \"%s\" for %s.", val, name);
		exit(1);
	} else if (result == LONG_MAX) {
		error ("Numeric argument (%ld) to big for %s.", result, name);
		exit(1);
	} else if (result == LONG_MIN) {
		error ("Numeric argument (%ld) to small for %s.", result, name);
		exit(1);
	}

	return (int) result;
}

/* print_db_notok() - Print an error message about slurmdbd
 *                    is unreachable or wrong cluster name.
 * IN  cname - char * cluster name
 * IN  isenv - bool  cluster name from env or from command line option.
 */
void print_db_notok(const char *cname, bool isenv)
{
	if (errno)
		error("There is a problem talking to the database: %m.  "
		      "Only local cluster communication is available, remove "
		      "%s or contact your admin to resolve the problem.",
		      isenv ? "SLURM_CLUSTERS from your environment" :
		      "--cluster from your command line");
	else if (!xstrcasecmp("all", cname))
		error("No clusters can be reached now. "
		      "Contact your admin to resolve the problem.");
	else
		error("'%s' can't be reached now, "
		      "or it is an invalid entry for %s.  "
		      "Use 'sacctmgr list clusters' to see available clusters.",
		      cname, isenv ? "SLURM_CLUSTERS" : "--cluster");
}

/*
 * parse_resv_flags() used to parse the Flags= option.  It handles
 * daily, weekly, static_alloc, part_nodes, and maint, optionally
 * preceded by + or -, separated by a comma but no spaces.
 *
 * flagstr IN - reservation flag string
 * msg IN - string to append to error message (e.g. function name)
 * RET equivalent reservation flag bits
 */
extern uint64_t parse_resv_flags(const char *flagstr, const char *msg)
{
	int flip;
	uint64_t outflags = 0;
	const char *curr = flagstr;
	int taglen = 0;

	while (*curr != '\0') {
		flip = 0;
		if (*curr == '+') {
			curr++;
		} else if (*curr == '-') {
			flip = 1;
			curr++;
		}
		taglen = 0;
		while (curr[taglen] != ',' && curr[taglen] != '\0')
			taglen++;

		if (xstrncasecmp(curr, "Maintenance", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_MAINT;
			else
				outflags |= RESERVE_FLAG_MAINT;
		} else if ((xstrncasecmp(curr, "Overlap", MAX(taglen,1))
			    == 0) && (!flip)) {
			curr += taglen;
			outflags |= RESERVE_FLAG_OVERLAP;
			/*
			 * "-OVERLAP" is not supported since that's the
			 * default behavior and the option only applies
			 * for reservation creation, not updates
			 */
		} else if (xstrncasecmp(curr, "Flex", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_FLEX;
			else
				outflags |= RESERVE_FLAG_FLEX;
		} else if (xstrncasecmp(curr, "Ignore_Jobs", MAX(taglen,1))
			   == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_IGN_JOB;
			else
				outflags |= RESERVE_FLAG_IGN_JOBS;
		} else if (xstrncasecmp(curr, "Daily", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_DAILY;
			else
				outflags |= RESERVE_FLAG_DAILY;
		} else if (xstrncasecmp(curr, "Weekday", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_WEEKDAY;
			else
				outflags |= RESERVE_FLAG_WEEKDAY;
		} else if (xstrncasecmp(curr, "Weekend", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_WEEKEND;
			else
				outflags |= RESERVE_FLAG_WEEKEND;
		} else if (xstrncasecmp(curr, "Weekly", MAX(taglen,1)) == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_WEEKLY;
			else
				outflags |= RESERVE_FLAG_WEEKLY;
		} else if (!xstrncasecmp(curr, "Any_Nodes", MAX(taglen,1)) ||
			   !xstrncasecmp(curr, "License_Only", MAX(taglen,1))) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_ANY_NODES;
			else
				outflags |= RESERVE_FLAG_ANY_NODES;
		} else if (xstrncasecmp(curr, "Static_Alloc", MAX(taglen,1))
			   == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_STATIC;
			else
				outflags |= RESERVE_FLAG_STATIC;
		} else if (xstrncasecmp(curr, "Part_Nodes", MAX(taglen, 2))
			   == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_PART_NODES;
			else
				outflags |= RESERVE_FLAG_PART_NODES;
		} else if (xstrncasecmp(curr, "PURGE_COMP", MAX(taglen, 2))
			   == 0) {
			curr += taglen;
			if (flip)
				outflags |= RESERVE_FLAG_NO_PURGE_COMP;
			else
				outflags |= RESERVE_FLAG_PURGE_COMP;
		} else if (!xstrncasecmp(curr, "First_Cores", MAX(taglen,1)) &&
			   !flip) {
			curr += taglen;
			outflags |= RESERVE_FLAG_FIRST_CORES;
		} else if (!xstrncasecmp(curr, "Time_Float", MAX(taglen,1)) &&
			   !flip) {
			curr += taglen;
			outflags |= RESERVE_FLAG_TIME_FLOAT;
		} else if (!xstrncasecmp(curr, "Replace", MAX(taglen, 1)) &&
			   !flip) {
			curr += taglen;
			outflags |= RESERVE_FLAG_REPLACE;
		} else if (!xstrncasecmp(curr, "Replace_Down", MAX(taglen, 8))
			   && !flip) {
			curr += taglen;
			outflags |= RESERVE_FLAG_REPLACE_DOWN;
		} else if (!xstrncasecmp(curr, "NO_HOLD_JOBS_AFTER_END",
					 MAX(taglen, 1)) && !flip) {
			curr += taglen;
			outflags |= RESERVE_FLAG_NO_HOLD_JOBS;
		} else {
			error("Error parsing flags %s.  %s", flagstr, msg);
			return 0xffffffff;
		}

		if (*curr == ',') {
			curr++;
		}
	}
	return outflags;
}

/* parse --compress for a compression type, set to default type if not found */
uint16_t parse_compress_type(const char *arg)
{
	/* if called with null string return default compression type */
	if (!arg) {
#if HAVE_LZ4
		return COMPRESS_LZ4;
#elif HAVE_LIBZ
		return COMPRESS_ZLIB;
#else
		error("No compression library available,"
		      " compression disabled.");
		return COMPRESS_OFF;
#endif
	}

	if (!strcasecmp(arg, "zlib"))
		return COMPRESS_ZLIB;
	else if (!strcasecmp(arg, "lz4"))
		return COMPRESS_LZ4;
	else if (!strcasecmp(arg, "none"))
		return COMPRESS_OFF;

	error("Compression type '%s' unknown, disabling compression support.",
	      arg);
	return COMPRESS_OFF;
}

int _validate_acctg_freq(const char *acctg_freq, const char *label)
{
	int i;
	char *save_ptr = NULL, *tok, *tmp;
	bool valid;
	int rc = SLURM_SUCCESS;

	if (!optarg)
		return rc;

	tmp = xstrdup(optarg);
	tok = strtok_r(tmp, ",", &save_ptr);
	while (tok) {
		valid = false;
		for (i = 0; i < PROFILE_CNT; i++)
			if (acct_gather_parse_freq(i, tok) != -1) {
				valid = true;
				break;
			}

		if (!valid) {
			error("Invalid %s specification: %s", label, tok);
			rc = SLURM_ERROR;
		}
		tok = strtok_r(NULL, ",", &save_ptr);
	}
	xfree(tmp);

	return rc;
}

/*
 * Format a tres_per_* argument
 * dest OUT - resulting string
 * prefix IN - TRES type (e.g. "gpu")
 * src IN - user input, can include multiple comma-separated specifications
 */
extern void xfmt_tres(char **dest, char *prefix, char *src)
{
	char *result = NULL, *save_ptr = NULL, *sep = "", *tmp, *tok;

	if (!src || (src[0] == '\0'))
		return;
	if (*dest) {
		result = xstrdup(*dest);
		sep = ",";
	}
	tmp = xstrdup(src);
	tok = strtok_r(tmp, ",", &save_ptr);
	while (tok) {
		xstrfmtcat(result, "%s%s:%s", sep, prefix, tok);
		sep = ",";
		tok = strtok_r(NULL, ",", &save_ptr);
	}
	xfree(tmp);
	*dest = result;
}

/*
 * Format a tres_freq argument
 * dest OUT - resulting string
 * prefix IN - TRES type (e.g. "gpu")
 * src IN - user input
 */
extern void xfmt_tres_freq(char **dest, char *prefix, char *src)
{
	char *result = NULL, *sep = "";

	if (!src || (src[0] == '\0'))
		return;
	if (*dest) {
		result = xstrdup(*dest);
		sep = ";";
	}
	xstrfmtcat(result, "%s%s:%s", sep, prefix, src);
	*dest = result;
}

static int _arg_set_error(const char *label, bool is_fatal,
			  const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, 1024, fmt, ap);
	va_end(ap);

	error("%s: %s.%s", label, buffer,
	      is_fatal ? "" : " Ignored.");
	if (is_fatal)
		exit(1);
	return SLURM_ERROR;
}


/* Read specified file's contents into a buffer.
 * Caller must xfree the buffer's contents */
static char *_read_file(const char *fname)
{
	int fd, i, offset = 0;
	struct stat stat_buf;
	char *file_buf;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fatal("Could not open burst buffer specification file %s: %m",
		      fname);
	}
	if (fstat(fd, &stat_buf) < 0) {
		fatal("Could not stat burst buffer specification file %s: %m",
		      fname);
	}
	file_buf = xmalloc(stat_buf.st_size);
	while (stat_buf.st_size > offset) {
		i = read(fd, file_buf + offset, stat_buf.st_size - offset);
		if (i < 0) {
			if (errno == EAGAIN)
				continue;
			fatal("Could not read burst buffer specification "
			      "file %s: %m", fname);
		}
		if (i == 0)
			break;	/* EOF */
		offset += i;
	}
	close(fd);
	return file_buf;
}

extern int arg_set_accel_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	if (strchr(arg, 'v'))
		sropt->accel_bind_type |= ACCEL_BIND_VERBOSE;
	if (strchr(arg, 'g'))
		sropt->accel_bind_type |= ACCEL_BIND_CLOSEST_GPU;
	if (strchr(arg, 'm'))
		sropt->accel_bind_type |= ACCEL_BIND_CLOSEST_MIC;
	if (strchr(arg, 'n'))
		sropt->accel_bind_type |= ACCEL_BIND_CLOSEST_NIC;

	return SLURM_SUCCESS;
}

extern int arg_set_account(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	xfree(opt->account);
	opt->account = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_acctg_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->acctg_freq);
	if (_validate_acctg_freq(arg, label) && is_fatal)
		exit(1);
	opt->acctg_freq = xstrdup(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_array(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;

	xfree(sbopt->array_inx);
	sbopt->array_inx = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_batch(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;

	xfree(sbopt->batch_features);
	sbopt->batch_features = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_bb(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->burst_buffer);
	opt->burst_buffer = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_bbf(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!arg)
		return SLURM_ERROR;
	if (sbopt) {
		/* sbatch only */
		xfree(sbopt->burst_buffer_file);
		sbopt->burst_buffer_file = _read_file(arg);
	} else {
		/* salloc and srun */
		xfree(opt->burst_buffer);
		opt->burst_buffer = _read_file(arg);
	}
	return SLURM_SUCCESS;
}
		
extern int arg_set_bcast(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	if (arg) {
		xfree(sropt->bcast_file);
		sropt->bcast_file = xstrdup(arg);
	}
	sropt->bcast_flag = true;
	return SLURM_SUCCESS;
}

extern int arg_set_begin(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	opt->begin = parse_time(arg, 0);
	if (opt->srun_opt) {
		/* srun has it's own error condition */
		if (errno == ESLURM_INVALID_TIME_VALUE)
			return _arg_set_error(label, is_fatal,
					      "invalid time specification %s",
					      arg);
	} else if (opt->begin == 0)
		/* salloc and sbatch error */
		return _arg_set_error(label, is_fatal,
				      "invalid time specification %s",
				      arg);
	return SLURM_SUCCESS;
}

extern int arg_set_bell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	salloc_opt_t *saopt = opt->salloc_opt;
	if (!saopt)
		return SLURM_SUCCESS;

	saopt->bell = BELL_ALWAYS;
	return SLURM_SUCCESS;
}

extern int arg_set_chdir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_workdir(opt, arg, label, is_fatal);
}

extern int arg_set_checkpoint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	char **interval_str = NULL;
	int *interval = NULL;
	int tmp = 0;
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;

	if (sbopt) {
		interval_str = &sbopt->ckpt_interval_str;
		interval = &sbopt->ckpt_interval;
	}
	if (sropt) {
		interval_str = &sropt->ckpt_interval_str;
		interval = &sropt->ckpt_interval;
	}
	if (!interval)
		return SLURM_SUCCESS;

	tmp = time_str2mins(arg);	
	if ((tmp < 0) && (tmp != INFINITE))
		return _arg_set_error(label, is_fatal, "invalid checkpoint interval specification: %s", arg);

	xfree(*interval_str);
	*interval_str = xstrdup(arg);
	*interval = tmp;
	return SLURM_SUCCESS;
}

extern int arg_set_checkpoint_dir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (sbopt) {
		xfree(sbopt->ckpt_dir);
		sbopt->ckpt_dir = xstrdup(arg);
	}
	if (sropt) {
		xfree(sropt->ckpt_dir);
		sropt->ckpt_dir = xstrdup(arg);
	}
	return SLURM_SUCCESS;
}

extern int arg_set_cluster(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_clusters(opt, arg, label, is_fatal);
}

extern int arg_set_cluster_constraint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	xfree(opt->c_constraints);
	opt->c_constraints = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_clusters(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->clusters);
	opt->clusters = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_comment(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	xfree(opt->comment);
	opt->comment = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_compress(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (sropt)
		sropt->compress = parse_compress_type(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_constraint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	xfree(opt->constraints);
	opt->constraints = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_contiguous(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->contiguous = true;
	return SLURM_SUCCESS;
}

extern int arg_set_core_spec(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	opt->core_spec = parse_int(label, arg, false);
	if (sropt)
		sropt->core_spec_set = true;

	return SLURM_SUCCESS;
}

extern int arg_set_cores_per_socket(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int max_val = 0;
	if (!arg)
		return SLURM_ERROR;

	get_resource_arg_range( arg, "cores-per-socket",
				&opt->cores_per_socket,
				&max_val, is_fatal );
	if ((opt->cores_per_socket == 1) &&
	    (max_val == INT_MAX))
		opt->cores_per_socket = NO_VAL;

	return SLURM_SUCCESS;
}

extern int arg_set_cpu_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	if (!arg)
		return SLURM_ERROR;

	xfree(sropt->cpu_bind);
	if (slurm_verify_cpu_bind(arg, &sropt->cpu_bind,
				  &sropt->cpu_bind_type, 0))
		return _arg_set_error(label, is_fatal, "invalid argument: %s", arg);

	sropt->cpu_bind_type_set = true;
	return SLURM_SUCCESS;
}

extern int arg_set_cpu_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

        if (cpu_freq_verify_cmdline(arg, &opt->cpu_freq_min,
			&opt->cpu_freq_max, &opt->cpu_freq_gov))
		return _arg_set_error(label, is_fatal, "invalid argument: %s", arg);

	return SLURM_SUCCESS;
}

extern int arg_set_cpus_per_gpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->cpus_per_gpu = parse_int(label, arg, is_fatal);
	return SLURM_SUCCESS;
}

extern int arg_set_cpus_per_task_int(slurm_opt_t *opt, int arg, const char *label, bool is_fatal) {
	if (opt->srun_opt && opt->cpus_set && (arg > opt->cpus_per_task)) {
		/* warn only for srun */
		info("Job step's --cpus-per-task value exceeds"
		     " that of job (%d > %d). Job step may "
		     "never run.", arg, opt->cpus_per_task);
	}
	if (arg <= 0)
		return _arg_set_error(label, is_fatal, "invalid number of cpus per task: %d", arg);

	if (opt->sbatch_opt)
		opt->sbatch_opt->pack_env->cpus_per_task = arg;
	opt->cpus_set = true;
	opt->cpus_per_task = arg;

	return SLURM_SUCCESS;
}

extern int arg_set_cpus_per_task(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int tmp_int = parse_int(label, arg, false);
	return arg_set_cpus_per_task_int(opt, tmp_int, label, is_fatal);
}

extern int arg_set_cpus_per_task_int(slurm_opt_t *opt, int arg, const char *label, bool is_fatal) {
	if (opt->srun_opt && opt->cpus_set && (arg > opt->cpus_per_task)) {
		/* warn only for srun */
		info("Job step's --cpus-per-task value exceeds"
		     " that of job (%d > %d). Job step may "
		     "never run.", arg, opt->cpus_per_task);
	}
	if (arg <= 0)
		return _arg_set_error(label, is_fatal, "invalid number of cpus per task: %d", arg);

	if (opt->sbatch_opt)
		opt->sbatch_opt->pack_env->cpus_per_task = arg;
	opt->cpus_set = true;
	opt->cpus_per_task = arg;

	return SLURM_SUCCESS;
}

extern int arg_set_deadline(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	opt->deadline = parse_time(arg, 0);
	if (errno == ESLURM_INVALID_TIME_VALUE)
		return _arg_set_error(label, is_fatal, "invalid time specification: %s", arg);

	return SLURM_SUCCESS;
}

extern int arg_set_debugger_test(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->debugger_test    = true;
	/* make other parameters look like debugger
	 * is really attached */
	sropt->parallel_debug   = true;
	sropt->max_threads     = 1;
	pmi_server_max_threads(sropt->max_threads);
	sropt->msg_timeout     = 15;

	return SLURM_SUCCESS;
}

extern int arg_set_delay_boot(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int tmp_int = 0;
	if (!arg)
		return SLURM_ERROR;

	tmp_int = time_str2secs(arg);
	if (tmp_int == NO_VAL)
		return _arg_set_error(label, is_fatal, "invalid argument: %s", arg);

	opt->delay_boot = (uint32_t) tmp_int;
	return SLURM_SUCCESS;
}

extern int arg_set_dependency(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->dependency);
	opt->dependency = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_disable_status(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->disable_status = true;
	return SLURM_SUCCESS;
}

extern int arg_set_distribution(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	task_dist_states_t dt;
	if (!arg)
		return SLURM_ERROR;

	if (opt->srun_opt && xstrcmp(arg, "unknown") == 0) {
		/* ignore it, passed from salloc */
		return SLURM_SUCCESS;
	}
	dt = verify_dist_type(arg, &opt->plane_size);
	if (dt == SLURM_DIST_UNKNOWN)
		return _arg_set_error(label, is_fatal, "distribution type not recognized: %s", arg);

	opt->distribution = dt;
	return SLURM_SUCCESS;
}

extern int arg_set_epilog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->epilog);
	sropt->epilog = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_error(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	if (sbopt) {
		xfree(sbopt->efname);
		if (xstrcasecmp(arg, "none") == 0)
			sbopt->efname = xstrdup("/dev/null");
		else
			sbopt->efname = xstrdup(arg);
	}
	if (sropt) {
		if (sropt->pty)
			return _arg_set_error(label,
					    "incompatible with --pty option",
					    "", is_fatal);
		xfree(sropt->efname);
		if (xstrcasecmp(arg, "none") == 0)
			sropt->efname = xstrdup("/dev/null");
		else
			sropt->efname = xstrdup(arg);
	}
	return SLURM_SUCCESS;
}

static bool _valid_node_list(char **node_list_pptr)
{
        int count = NO_VAL;

        /* If we are using Arbitrary and we specified the number of
           procs to use then we need exactly this many since we are
           saying, lay it out this way!  Same for max and min nodes.
           Other than that just read in as many in the hostfile */
	if (opt.ntasks_set)
		count = opt.ntasks;
	else if (opt.nodes_set) {
		if (opt.max_nodes)
			count = opt.max_nodes;
		else if (opt.min_nodes)
			count = opt.min_nodes;
	}

	return verify_node_list(node_list_pptr, opt.distribution, count);
}

extern int arg_set_exclude(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->exc_nodes);
	opt->exc_nodes = xstrdup(arg);
	if (!_valid_node_list(&opt->exc_nodes))
		return _arg_set_error(label, is_fatal, "invalid node list: %s", arg);

	return SLURM_SUCCESS;
}

extern int arg_set_exclusive(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (arg == NULL || arg[0] == '\0') {
		if (sropt)
			sropt->exclusive = true;
		opt->shared = JOB_SHARED_NONE;
	} else if (!xstrcasecmp(arg, "user")) {
		opt->shared = JOB_SHARED_USER;
	} else if (!xstrcasecmp(arg, "mcs")) {
		opt->shared = JOB_SHARED_MCS;
	} else {
		return _arg_set_error(label, is_fatal, "invalid exclusive option: %s", arg);
	}
	return SLURM_SUCCESS;
}

extern int arg_set_export(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (sbopt) {
		xfree(sbopt->export_env);
		sbopt->export_env = xstrdup(arg);
		if (!xstrcasecmp(sbopt->export_env, "ALL"))
			; /* srun ignores "ALL", it is the default */
		else
			setenv("SLURM_EXPORT_ENV", sbopt->export_env, 0);
	}
	if (sropt) {
		xfree(sropt->export_env);
		sropt->export_env = xstrdup(arg);
	}
	return SLURM_SUCCESS;
}

extern int arg_set_export_file(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	xfree(sbopt->export_file);
	sbopt->export_file = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_extra_node_info(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	opt->extra_set = verify_socket_core_thread_count(
				arg,
				&opt->sockets_per_node,
				&opt->cores_per_socket,
				&opt->threads_per_core,
				&sropt->cpu_bind_type);
	if (opt->extra_set == false)
		return _arg_set_error(label, is_fatal, "invalid resource allocation: %s", arg);

	if (sropt)
		sropt->cpu_bind_type_set = true;
	opt->threads_per_core_set = true;

	return SLURM_SUCCESS;
}

static void _proc_get_user_env(char *optarg) {
	char *end_ptr;
	if ((optarg[0] >= '0') && (optarg[0] <= '9'))
		opt.get_user_env_time = strtol(optarg, &end_ptr, 10);
	else {
		opt.get_user_env_time = 0;
		end_ptr = optarg;
	}

	if ((end_ptr == NULL) || (end_ptr[0] == '\0'))
		return;
	if ((end_ptr[0] == 's') || (end_ptr[0] == 'S'))
		opt.get_user_env_mode = 1;
	else if ((end_ptr[0] == 'l') || (end_ptr[0] == 'L'))
		opt.get_user_env_mode = 2;
}

extern int arg_set_get_user_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (opt->srun_opt) {
		error("--get-user-env is no longer supported in srun, use "
		      "sbatch");
		return SLURM_SUCCESS;
	}
	if (arg)
		_proc_get_user_env(arg);
	else
		opt->get_user_env_time = 0;

	return SLURM_SUCCESS;
}

extern int arg_set_gid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	if (getuid() != 0)
		return _arg_set_error(label, "only permitted by root user", "", is_fatal);
	if (opt->egid != (gid_t) -1)
		return _arg_set_error(label, "duplicate option", "", is_fatal);
	if (gid_from_string(arg, &opt->egid) < 0)
		return _arg_set_error(label, "invalid option", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_gpu_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpu_bind);
	opt->gpu_bind = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gpu_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpu_bind);
	opt->gpu_freq = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gpus(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpus);
	opt->gpus = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gpus_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpu_per_node);
	opt->gpus_per_node = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gpus_per_task(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpus_per_task);
	opt->gpus_per_task = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gres(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	if (!xstrcasecmp(arg, "help") ||
	    !xstrcasecmp(arg, "list")) {
		print_gres_help();
		exit(0);
	}
	xfree(opt->gres);
	opt->gres = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gres_flags(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	if (!xstrcasecmp(optarg, "disable-binding"))
		opt->job_flags |= GRES_DISABLE_BIND;
	if (!xstrcasecmp(arg, "enforce-binding"))
		opt->job_flags |= GRES_ENFORCE_BIND;
	else
		return _arg_set_error(label, "invalid gres-flags specification", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_hint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	cpu_bind_type_t *cpu_bind_type = NULL;

	if (!arg)
		return SLURM_ERROR;
	if (sropt)
		cpu_bind_type = sropt->cpu_bind_type;

	/* Keep this logic after other options filled in */
	if (!opt->hint_set && !opt->ntasks_per_core_set &&
	    !opt->threads_per_core_set) {

		if (verify_hint(arg,
				&opt->sockets_per_node,
				&opt->cores_per_socket,
				&opt->threads_per_core,
				&opt->ntasks_per_core,
				cpu_bind_type)) {
			return _arg_set_error(label, "invalid argument", arg, is_fatal);
		}
		opt->hint_set = true;
		opt->ntasks_per_core_set  = true;
		opt->threads_per_core_set = true;
	}
	return SLURM_SUCCESS;
}

extern int arg_set_hold(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->hold = true;
	return SLURM_SUCCESS;
}

extern int arg_set_ignore_pbs(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	ignore_pbs = 1;
	return SLURM_SUCCESS;
}

extern int arg_set_immediate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (opt->sbatch_opt) {
		info("%s option is not supported for the sbatch command, ignored", label);
		return SLURM_SUCCESS;
	}

	/* salloc and srun */
	if (arg)
		opt->immediate = parse_int(label, arg, true);
	else
		opt->immediate = DEFAULT_IMMEDIATE;

	return SLURM_SUCCESS;
}

extern int arg_set_input(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	if (sbopt) {
		xfree(sbopt->ifname);
		if (xstrcasecmp(arg, "none") == 0)
			sbopt->ifname = xstrdup("/dev/null");
		else
			sbopt->ifname = xstrdup(arg);
	}
	if (sropt) {
		if (sropt->pty) {
			fatal("--input incompatible with --pty option");
			return SLURM_ERROR; /* not necessary, fatal()d */
		}
		xfree(sropt->ifname);
		if (xstrcasecmp(arg, "none") == 0)
			sropt->ifname = xstrdup("/dev/null");
		else
			sropt->ifname = xstrdup(arg);
	}
	return SLURM_SUCCESS;
}

extern int arg_set_job_name(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	if (sropt)
		sropt->job_name_set_cmd = true;

	xfree(opt->job_name);
	opt->job_name = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_job_name_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int rc = arg_set_job_name(opt, arg, label, is_fatal);
	srun_opt_t *sropt = opt->srun_opt;
	if (rc == SLURM_SUCCESS && sropt) {
		sropt->job_name_set_cmd = false;
		sropt->job_name_set_env = true;
	}
	return rc;
}

extern int arg_set_jobid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	opt->jobid = parse_int(label, arg, true);
	if (!opt->salloc_opt) {
		/* TODO this seems wrong, my guess is that salloc should also 
		   set jobid_set */
		info("WARNING: Creating SLURM job allocation from within "
		     "another allocation");
		info("WARNING: You are attempting to initiate a second job");
		opt->jobid_set = true;
	}
	return SLURM_SUCCESS;
}

extern int arg_set_join(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	/* Vestigial option */
	return SLURM_SUCCESS;
}

extern int arg_set_kill_command(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	salloc_opt_t *saopt = opt->salloc_opt;
	if (!saopt)
		return SLURM_SUCCESS;

	if (arg) {
		/* argument is optional */
		saopt->kill_command_signal = sig_name2num(arg);
		if (saopt->kill_command_signal == 0) {
			return SLURM_ERROR;
		}
	}
	saopt->kill_command_signal_set = true;

	return SLURM_SUCCESS;
}

extern int arg_set_kill_on_bad_exit(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	if (arg)
		sropt->kill_bad_exit = strtol(arg, NULL, 10);
	else
		sropt->kill_bad_exit = 1;

	return SLURM_SUCCESS;
}

extern int arg_set_kill_on_invalid_dep(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!opt->sbatch_opt)
		return SLURM_SUCCESS;

	if (xstrcasecmp(arg, "yes") == 0)
		opt->job_flags |= KILL_INV_DEP;
	if (xstrcasecmp(arg, "no") == 0)
		opt->job_flags |= NO_KILL_INV_DEP;

	return SLURM_SUCCESS;
}

extern int arg_set_label(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->labelio = true;
	return SLURM_SUCCESS;
}

extern int arg_set_launch_cmd(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->launch_cmd = true;
	return SLURM_SUCCESS;
}

extern int arg_set_launcher_opts(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->launcher_opts);
	sropt->launcher_opts = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_licenses(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	xfree(opt->licenses);
	opt->licenses = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_mail_type(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	opt->mail_type |= parse_mail_type(arg);
	if (opt->mail_type == INFINITE16) {
		error("--%s=%s invalid", label, arg);
		return SLURM_ERROR;
	}

	return SLURM_SUCCESS;
}

extern int arg_set_mail_user(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	xfree(opt->mail_user);
	opt->mail_user = xstrdup(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_mcs_label(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	xfree(opt->mcs_label);
	opt->mcs_label = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_mem(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int64_t mbytes = 0;
	if (!arg)
		return SLURM_ERROR;
	mbytes = (int64_t) str_to_mbytes2(arg);
	return arg_set_mem_mb(opt, mbytes, label, is_fatal);
}

extern int arg_set_mem_mb(slurm_opt_t *opt, int64_t mbytes, const char *label) {
	opt->pn_min_memory = mbytes);
	if (opt->srun_opt) {
		/* only srun does this */
		opt->mem_per_cpu = NO_VAL64;
	}
	if (opt->pn_min_memory < 0)
		return _arg_set_error(label, "invalid memory constraint", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_mem_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	if (opt->srun_opt) {
		/* TODO only srun does this at present */
		xfree(opt->mem_bind);
	}
	if (slurm_verify_mem_bind(arg, &opt->mem_bind, &opt->mem_bind_type))
		return _arg_set_error(label, "invalid argument", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_cpu_mb(slurm_opt_t *opt, int64_t mbytes, const char *label) {
	opt->mem_per_cpu = mbytes;
	if (opt->srun_opt) {
		/* only srun does this */
		opt->pn_min_memory = NO_VAL64;
	}
	if (opt->mem_per_cpu < 0)
		return _arg_set_error(label, "invalid memory constraint", arg, is_fatal);;
	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_cpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int64_t mem_per_cpu = 0;
	if (!arg)
		return SLURM_ERROR;
	mem_per_cpu = (int64_t) str_to_mbytes2(arg);
	return arg_set_mem_per_cpu_mb(opt, mem_per_cpu, label, is_fatal);
}

extern int arg_set_mem_per_gpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int64_t mbytes = 0;
	if (!arg)
		return SLURM_ERROR;
	mbytes = (int64_t) str_to_mbytes2(arg);
	return arg_set_mem_per_gpu_mb(opt, mbytes, label, is_fatal);
}

extern int arg_set_mem_per_gpu_mb(slurm_opt_t *opt, int64_t mbytes, const char *label, bool is_fatal) {
	opt->mem_per_gpu = mbytes;
	if (opt->mem_per_gpu < 0)
		return _arg_set_error(label, "invalid mem-per-gpu constraint", arg, is_fatal);

	return SLURM_SUCCESS;
}


extern int arg_set_mincores(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	verbose("mincores option has been deprecated, use cores-per-socket");
	opt->cores_per_socket = parse_int(label, arg, true);
	if (opt->cores_per_socket < 0)
		return _arg_set_error(label, "invalid argument", arg, is_fatal);
	return SLURM_SUCCESS;

}

extern int arg_set_mincpus(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int temp = 0;
	if (!arg)
		return SLURM_ERROR;
	temp = parse_int(label, arg, true);
	return arg_set_mincpus_int(opt, temp, label, is_fatal);
}

extern int arg_set_mincpus_int(slurm_opt_t *opt, int arg, const char *label, bool is_fatal) {
	opt->pn_min_cpus = arg;
	if (opt->srun_opt)
		return SLURM_SUCCESS;

	/* DMJ: srun does not give this warning, only salloc/sbatch.
	 * unless, I misunderstand the purpose of parse_int, I think
	 * this warning may be superflous */

	if (opt->pn_min_cpus < 0)
		return SLURM_ERROR;
	return SLURM_SUCCESS;
}

extern int arg_set_minsockets(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	verbose("minsockets option has been deprecated, use sockets-per-node");
	opt->sockets_per_node = parse_int(label, arg, true);
	if (opt->sockets_per_node < 0)
		return _arg_set_error(label, "invalid argument", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_minthreads(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	verbose("minthreads option has been deprecated, use "
		"threads-per-core");
	opt->threads_per_core = parse_int(label, arg, true);
	if (opt->threads_per_core < 0)
		return _arg_set_error(label", invalid argument", arg, is_fatal);

	opt->threads_per_core_set = true;

	return SLURM_SUCCESS;
}

extern int arg_set_mpi(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->mpi_type);
	opt->mpi_type = xstrdup(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_msg_timeout(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->msg_timeout = parse_int(label, arg, true);
	return SLURM_SUCCESS;
}

extern int arg_set_multi_prog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->multi_prog = true;
	return SLURM_SUCCESS;
}

extern int arg_set_network(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->network);
	opt->network = xstrdup(arg);
	if (sropt) {
		setenv("SLURM_NETWORK", opt->network, 1);
		sropt->network_set_env = false;
	}
	return SLURM_SUCCESS;
}

extern int arg_set_network_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int rc = arg_set_network(opt, arg, label, is_fatal);
	if (rc == SLURM_SUCCESS && opt->srun_opt)
		opt->srun_opt->network_set_env = true;
	return rc;
}

extern int arg_set_nice(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	long long tmp_nice;
	if (arg)
		tmp_nice = strtoll(arg, NULL, 10);
	else
		tmp_nice = 100;
	if (llabs(tmp_nice) > (NICE_OFFSET - 3)) {
		error("Nice value out of range (+/- %u). Value "
		      "ignored", NICE_OFFSET - 3);
		tmp_nice = 0;
	}
	if (tmp_nice < 0) {
		uid_t my_uid = getuid();
		if ((my_uid != 0) &&
		    (my_uid != slurm_get_slurm_user_id())) {
			error("Nice value must be "
			      "non-negative, value ignored");
			tmp_nice = 0;
		}
	}
	opt->nice = (int) tmp_nice;

	return SLURM_SUCCESS;
}

extern int arg_set_no_allocate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->no_alloc = true;
	uname(&name);
	if (xstrcasecmp(name.sysname, "AIX") == 0)
		opt->network = xstrdup("ip");
	return SLURM_SUCCESS;
}

extern int arg_set_no_bell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	salloc_opt_t *saopt = opt->salloc_opt;
	if (!saopt)
		return SLURM_SUCCESS;

	saopt->bell = BELL_NEVER;
	return SLURM_SUCCESS;
}

extern int arg_set_no_kill(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (arg && (!xstrcasecmp(arg, "off") || !xstrcasecmp(arg, "no"))) {
		opt.no_kill = false;
	} else
		opt->no_kill = true;
	return SLURM_SUCCESS;
}

extern int arg_set_no_requeue(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	sbopt->requeue = 0;
	return SLURM_SUCCESS;
}

extern int arg_set_no_shell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	salloc_opt_t *saopt = opt->salloc_opt;
	if (!saopt)
		return SLURM_SUCCESS;

	saopt->no_shell = true;

	return SLURM_SUCCESS;
}

extern int arg_set_nodefile(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	/* skip if srun */
	if (!arg)
		return SLURM_ERROR;
	if (opt->srun_opt)
		return SLURM_SUCCESS;

	xfree(opt->nodelist);
	tmp = slurm_read_hostfile(arg, 0);
	if (tmp != NULL) {
		opt->nodelist = xstrdup(tmp);
		free(tmp);
	} else {
		return _arg_set_error(label, "invalid node file", arg, is_fatal);
	}

	return SLURM_SUCCESS;
}

extern int arg_set_nodelist(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->nodelist);
	opt->nodelist = xstrdup(arg);

	if (!_valid_node_list(opt->nodelist))
		return _arg_set_error(label, "invalid argument", arg, is_fatal);
	return SLURM_SUCCESS;
}

extern int arg_set_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	if (sropt) {
		sropt->nodes_set_opt = get_resource_arg_range(arg, "requested node count",
					     &opt->min_nodes, &opt->max_nodes,
					     is_fatal);
		if (sropt->nodes_set_opt == false)
			return _arg_set_error(label, "invalid node count", arg, is_fatal);

		opt->nodes_set = true;
		return SLURM_SUCCESS;
	}

	/* for sbatch and salloc */
	opt->nodes_set = verify_node_count(arg, &opt->min_nodes,
					   &opt->max_nodes);
	if (opt->nodes_set == false)
		return _arg_set_error(label, "invalid node count", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_nodes_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	int rc = arg_set_nodes(opt, arg, label, is_fatal);
	if (rc == SLURM_SUCCESS && sropt) {
		sropt->nodes_set_opt = false;
		sropt->nodes_set_env = true;
	}
	return rc;
}

extern int arg_set_ntasks_int(slurm_opt_t *opt, int ntasks, const char *label) {
	opt->ntasks = ntasks;
	if (sbopt)
		sbopt->pack_env->ntasks = opt->ntasks;
}

extern int arg_set_ntasks(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	return arg_set_ntasks_int(opt, parse_int(label, arg, true), label, is_fatal);
}

extern int arg_set_ntasks_per_core(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!arg)
		return SLURM_ERROR;

	opt->ntasks_per_core = parse_int(label, arg, true);
	opt->ntasks_per_core_set = true;
	if (sbopt)
		sbopt->pack_env->ntasks_per_core = opt->ntasks_per_core;

	return SLURM_SUCCESS;
}

extern int arg_set_ntasks_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!arg)
		return SLURM_ERROR;

	opt->ntasks_per_node = parse_int(label, arg, true);
	if (sbopt && opt->ntasks_per_node > 0)
		sbopt->pack_env->ntasks_per_node = opt->ntasks_per_node;

	return SLURM_SUCCESS;
}

extern int arg_set_ntasks_per_socket(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!arg)
		return SLURM_ERROR;

	opt->ntasks_per_socket = parse_int(label, arg, true);
	if (sbopt)
		sbopt->pack_env->ntasks_per_socket = opt->ntasks_per_socket;

	return SLURM_SUCCESS;
}

extern int arg_set_open_mode(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	uint8_t *open_mode = NULL;

	if (!arg)
		return SLURM_ERROR;

	if (sbopt)
		open_mode = &sbopt->open_mode;
	if (sropt)
		open_mode = &sropt->open_mode;

	if (open_mode) {
		if ((arg[0] == 'a') || (arg[0] == 'A'))
			*open_mode = OPEN_MODE_APPEND;
		else if ((arg[0] == 't') || (arg[0] == 'T'))
			*open_mode = OPEN_MODE_TRUNCATE;
		else
			return _arg_set_error(label, "invalid argument", arg, is_fatal);

		if (sbopt)
			setenvf(NULL, "SLURM_OPEN_MODE", arg);
	}

	return SLURM_SUCCESS;
}

extern int arg_set_output(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;

	if (!arg)
		return SLURM_ERROR;

	if (sbopt) {
		xfree(sbopt->ofname);
		if (xstrcasecmp(arg, "none") == 0)
			sbopt->ofname = xstrdup("/dev/null");
		else
			sbopt->ofname = xstrdup(arg);
	}

	if (sropt) {
		if (sropt->pty)
			return SLURM_ERROR;

		xfree(sropt->ofname);
		if (xstrcasecmp(arg, "none") == 0)
			sropt->ofname = xstrdup("/dev/null");
		else
			sropt->ofname = xstrdup(arg);
	}

	return SLURM_SUCCESS;
}

extern int arg_set_overcommit(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->overcommit = true;
	return SLURM_SUCCESS;
}

extern int arg_set_oversubscribe(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_share(opt, arg, label, is_fatal);
}

extern int arg_set_parsable(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;

	sbopt->parsable = true;
	return SLURM_SUCCESS;
}

extern int arg_set_partition(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->partition);
	opt->partition = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_power(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	opt->power_flags = power_flags_id(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_preserve_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->preserve_env = true;
	return SLURM_SUCCESS;
}

extern int arg_set_preserve_slurm_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->preserve_env = true;
	return SLURM_SUCCESS;
}

extern int arg_set_priority(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	long long priority = 0;
	if (strcasecmp(arg, "TOP") == 0) {
		opt->priority = NO_VAL - 1;
	} else {
		priority = strtoll(arg, NULL, 10);
		if (priority < 0 || priority >= NO_VAL) {
			error("Priority must be >=0 and < %i", NO_VAL);
			return SLURM_ERROR;
		}

		opt->priority = priority;
	}

	return SLURM_SUCCESS;
}

extern int arg_set_profile(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	opt->profile = acct_gather_profile_from_string(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_prolog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->prolog);
	sropt->prolog = xstrdup(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_propagate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (sbopt) {
		xfree(sbopt->propagate);
		if (arg)
			sbopt->propagate = xstrdup(arg);
		else
			sbopt->propagate = xstrdup("ALL");
	}
	if (sropt) {
		xfree(sropt->propagate);
		if (arg)
			sropt->propagate = xstrdup(arg);
		else
			sropt->propagate = xstrdup("ALL");
	}
	return SLURM_SUCCESS;
}

extern int arg_set_pty(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	char *tmp_str = NULL;
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;
#ifdef HAVE_PTY_H
	sropt->pty = true;
	sropt->unbuffered = true;	/* implicit */
	if (sropt->ifname)
		tmp_str = "--input";
	else if (sropt->ofname)
		tmp_str = "--output";
	else if (sropt->efname)
		tmp_str = "--error";
	else
		tmp_str = NULL;
	if (tmp_str) {
		error("%s incompatible with --pty option",
		      tmp_str);
		if (is_fatal)
			exit(1);
		return SLURM_ERROR;
	}
#else
	error("--pty not currently supported on this system "
	      "type, ignoring option");
#endif

	return SLURM_SUCCESS;
}

extern int arg_set_qos(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->qos);
	opt->qos = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_quiet(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->quiet++;
	return SLURM_SUCCESS;
}

extern int arg_set_quit_on_interrupt(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;
	sropt->quit_on_intr = true;
	return SLURM_SUCCESS;
}

extern int arg_set_reboot(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
#if defined HAVE_BG
	/* sbatch and salloc get the warning */
	if (!opt->srun_opt) {
		info("WARNING: If your job is smaller than the block "
		     "it is going to run on and other jobs are "
		     "running on it the --reboot option will not be "
		     "honored.  If this is the case, contact your "
		     "admin to reboot the block for you.");
	}
#endif
	opt->reboot = true;
	return SLURM_SUCCESS;
}

extern int arg_set_relative(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;
	if (!arg)
		return SLURM_ERROR;

	sropt->relative = parse_int(label, arg, false);
	sropt->relative_set = true;
	return SLURM_SUCCESS;
}

extern int arg_set_requeue(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;

	sbopt->requeue = 1;
	return SLURM_SUCCESS;
}

extern int arg_set_reservation(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->reservation);
	opt->reservation = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_restart_dir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->restart_dir);
	sropt->restart_dir = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_resv_ports(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	if (arg)
		sropt->resv_port_cnt = strtol(arg, NULL, 10);
	else
		sropt->resv_port_cnt = 0;

	return SLURM_SUCCESS;
}

extern int arg_set_runjob_opts(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_launcher_opts(opt, arg, label, is_fatal);
}

extern int arg_set_share(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->shared = 1;
	return SLURM_SUCCESS;
}

extern int arg_set_signal(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	if (get_signal_opts(arg, &opt->warn_signal,
			    &opt->warn_time, &opt->warn_flags))
		return _arg_set_error(label, "invalid signal specification", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_slurmd_debug(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;
	if (!arg)
		return SLURM_ERROR;

	if (isdigit(arg[0]))
		sropt->slurmd_debug = parse_int(label, arg, false);
	else
		sropt->slurmd_debug = log_string2num(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_sockets_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int max_val = 0;
	if (!arg)
		return SLURM_ERROR;

	get_resource_arg_range( arg, "sockets-per-node",
				&opt->sockets_per_node,
				&max_val, is_fatal);
	if ((opt->sockets_per_node == 1) &&
	    (max_val == INT_MAX))
		opt->sockets_per_node = NO_VAL;

	return SLURM_SUCCESS;
}

extern int arg_set_spread_job(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->job_flags |= SPREAD_JOB;

	return SLURM_SUCCESS;
}

extern int arg_setcomp_req_switch(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->req_switch = parse_int(label, arg, true);
	return SLURM_SUCCESS;
}

extern int arg_setcomp_req_wait4switch(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->wait4switch = time_str2secs(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_switches(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int rc = SLURM_SUCCESS;
	char *pos_delimit = strstr(arg,"@");
	if (!arg)
		return SLURM_ERROR;
	if (pos_delimit != NULL) {
		pos_delimit[0] = '\0';
		pos_delimit++;
		rc = arg_setcomp_req_wait4switch(opt, pos_delimit, label, is_fatal);
		if (rc)
			return rc;
	}
	rc = arg_setcomp_req_switch(opt, arg, label, is_fatal);
	return rc;
}

extern int arg_set_task_epilog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->task_epilog);
	sropt->task_epilog = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_task_prolog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;
	if (!sropt)
		return SLURM_SUCCESS;

	xfree(sropt->task_prolog);
	sropt->task_prolog = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_tasks(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_ntasks(opt, arg, label, is_fatal);
}

extern int arg_set_tasks_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	return arg_set_ntasks_per_node(opt, arg, label, is_fatal);
}

extern int arg_set_test_only(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (sbopt)
		sbopt->test_only = true;

	if (sropt)
		sropt->test_only = true;

	return SLURM_SUCCESS;
}

extern int arg_set_thread_spec(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	opt->core_spec = parse_int(label, arg, true) | CORE_SPEC_THREAD;
	return SLURM_SUCCESS;
}

extern int arg_set_threads(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;
	if (!arg)
		return SLURM_ERROR;
		
	sropt->max_threads = parse_int(label, arg, true);
	pmi_server_max_threads(sropt->max_threads);
	return SLURM_SUCCESS;
}

extern int arg_set_threads_per_core(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int max_val = 0;
	if (!arg)
		return SLURM_ERROR;

	get_resource_arg_range(arg, "threads-per-core", &opt->threads_per_core,
			       &max_val, is_fatal);
	if ((opt->threads_per_core == 1) && (max_val == INT_MAX))
		opt->threads_per_core = NO_VAL;

	opt->threads_per_core_set = true;
	return SLURM_SUCCESS;
}

extern int arg_set_time(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int time_limit = 0;
	if (!arg)
		return NULL;

	time_limit = time_str2mins(arg);
	if ((time_limit < 0) && (time_limit != INFINITE))
		return _arg_set_error(label, "invalid time limit specification", arg, is_fatal);

	if (time_limit == 0)
		time_limit = INFINITE;

	xfree(opt->time_limit_str);
	opt->time_limit_str = xstrdup(arg);
	opt->time_limit = time_limit;

	return SLURM_SUCCESS;
}

extern int arg_set_time_min(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int time_min = 0;
	if (!arg)
		return SLURM_ERROR;

	time_min = time_str2mins(arg);
	if ((time_min < 0) && (time_min != INFINITE))
		return _arg_set_error(label, "invalid time limit specification", arg, is_fatal);

	xfree(opt->time_min_str);
	opt->time_min_str = xstrdup(arg);
	opt->time_min = time_min;

	return SLURM_SUCCESS;
}

extern int arg_set_tmp(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	long tmp = 0;
	if (!arg)
		return SLURM_ERROR;
	tmp = str_to_mbytes2(arg);
	return arg_set_tmp_mb(opt, tmp, label, is_fatal);
}

extern int arg_set_tmp_mb(slurm_opt_t *opt, long mbytes, const char *label, bool is_fatal) {
	opt->pn_min_tmp_disk = mbytes;
	if (opt->pn_min_tmp_disk < 0)
		return _arg_set_error(label, "invalid tmp value", "", is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_tres_per_job(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->tres_per_job);
	opt->tres_per_job = xstrdup(arg);

	return SLURM_SUCCESS;
}

extern int arg_set_uid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERRROR;
	if (getuid() != 0)
		return _arg_set_error(label, "only permitted by root user", "", is_fatal);
	if (opt->euid != (uid_t) -1)
		return _arg_set_error(label, "duplicate option", "", is_fatal);
	if (uid_from_string(arg, &opt->euid) < 0)
		return _arg_set_error(label, "invalid argument", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_umask(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;
	sbopt->umask = strtol(arg, NULL, 0);
	if ((sbopt->umask < 0) || (sbopt->mask > 0777)) {
		error("Invalid umask ignored");
		sbopt->umask = -1;
	}
	return SLURM_SUCCESS;
}

extern int arg_set_unbuffered(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!sropt)
		return SLURM_SUCCESS;

	sropt->unbuffered = true;
	return SLURM_SUCCESS;
}

extern int arg_set_use_min_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	opt->job_flags |= USE_MIN_NODES;

	return SLURM_SUCCESS;
}

extern int arg_set_verbose(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	char *end = NULL;
	if (arg) {
                if (arg[0] != '\0') {
                        opt->verbose = (int) strtol(arg, &end, 10);
                        if (!(end && *end == '\0'))
				return SLURM_ERROR;
                }
	} else {
		opt->verbose++;
	}
	return SLURM_SUCCESS;
}

extern int arg_set_wait(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;

	if (sbopt) {
		if (arg)
			sbopt->wait = parse_bool(arg);
		else
			sbopt->wait = true;
		return SLURM_SUCCESS;
	}
	if (!arg)
		return SLURM_ERROR;

	if (opt->salloc_opt) {
		verbose("wait option has been deprecated, use immediate option");
		opt->immediate = parse_int(label, arg, true);
	}
	if (sropt)
		sropt->max_wait = parse_int(label, arg, false);
	return SLURM_SUCCESS;
}

extern int arg_set_wait_all_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	salloc_opt_t *saopt = opt->salloc_opt;
	sbatch_opt_t *sbopt = opt->sbatch_opt;

	/* TODO: why doesn't parse_int solve this problem? */
	if (saopt) {
		if ((arg[0] < '0') || (arg[0] > '9')) {
			error("%s: invalid value %s.%s", label, arg, is_fatal ? "" : " Ignored.");
			if (is_fatal)
				exit(1);
			return SLURM_ERROR;
		}
		saopt->wait_all_nodes = strtol(arg, NULL, 10);
	}
	if (sbopt) {
		if ((arg[0] < '0') || (arg[0] > '9')) {
			error("%s: invalid value %s.%s", label, arg, is_fatal ? "" : " Ignored.");
			if (is_fatal)
				exit(1);
			return SLURM_ERROR;
		}
		sbopt->wait_all_nodes = strtol(arg, NULL, 10);
	}

	return SLURM_SUCCESS;
}

extern int arg_set_wckey(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->wckey);
	opt->wckey = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_workdir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	srun_opt_t *sropt = opt->srun_opt;
	if (!arg)
		return SLURM_ERROR;

	/* not allowed for salloc */
	if (opt->salloc_opt)
		return SLURM_SUCCESS;

	xfree(opt->cwd);
	if (is_full_path(arg))
		opt->cwd = xstrdup(arg);
	else
		opt->cwd = make_full_path(arg);

	if (sropt)
		sropt->cwd_set = true;

	return SLURM_SUCCESS;
}

extern int arg_set_wrap(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (!sbopt)
		return SLURM_SUCCESS;

	xfree(opt->job_name);
	xfree(sbopt->wrap);
	opt->job_name = xstrdup("wrap");
	sbopt->wrap = xstrdup(optarg);
	return SLURM_SUCCESS;
}

extern int arg_set_x11(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (arg)
		opt->x11 = x11_str2flags(arg);
	else if (opt->sbatch_opt)
		opt->x11 = X11_FORWARD_BATCH;
	else
		opt->x11 = X11_FORWARD_ALL;

	return SLURM_SUCCESS;
}
