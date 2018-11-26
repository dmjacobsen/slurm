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
#include <getopt.h>

#include "src/api/pmi_server.h"
#include "src/common/cpu_frequency.h"
#include "src/common/gres.h"
#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/parse_time.h"
#include "src/common/plugstack.h"
#include "src/common/proc_args.h"
#include "src/common/slurm_acct_gather_profile.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_resource_info.h"
#include "src/common/uid.h"
#include "src/common/util-net.h"
#include "src/common/x11_util.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#if 0
#define LONG_OPT_MEM_BIND    0x102
#define LONG_OPT_JOBID       0x105
#define LONG_OPT_TMP         0x106
#define LONG_OPT_MEM         0x107
#define LONG_OPT_MINCPU      0x108
#define LONG_OPT_CONT        0x109
#define LONG_OPT_UID         0x10a
#define LONG_OPT_GID         0x10b
#define LONG_OPT_MINSOCKETS  0x10c
#define LONG_OPT_MINCORES    0x10d
#define LONG_OPT_MINTHREADS  0x10e
#define LONG_OPT_CORE	     0x10f
#define LONG_OPT_EXCLUSIVE   0x111
#define LONG_OPT_BEGIN       0x112
#define LONG_OPT_MAIL_TYPE   0x113
#define LONG_OPT_MAIL_USER   0x114
#define LONG_OPT_NICE        0x115
#define LONG_OPT_BELL        0x116
#define LONG_OPT_NO_BELL     0x117
#define LONG_OPT_COMMENT     0x118
#define LONG_OPT_REBOOT      0x119
#define LONG_OPT_BLRTS_IMAGE     0x120
#define LONG_OPT_LINUX_IMAGE     0x121
#define LONG_OPT_MLOADER_IMAGE   0x122
#define LONG_OPT_RAMDISK_IMAGE   0x123
#define LONG_OPT_NOSHELL         0x124
#define LONG_OPT_GET_USER_ENV    0x125
#define LONG_OPT_NETWORK         0x126
#define LONG_OPT_BURST_BUFFER_SPEC  0x128
#define LONG_OPT_BURST_BUFFER_FILE  0x129
#define LONG_OPT_SOCKETSPERNODE  0x130
#define LONG_OPT_CORESPERSOCKET  0x131
#define LONG_OPT_THREADSPERCORE  0x132
#define LONG_OPT_NTASKSPERNODE   0x136
#define LONG_OPT_NTASKSPERSOCKET 0x137
#define LONG_OPT_NTASKSPERCORE   0x138
#define LONG_OPT_MEM_PER_CPU     0x13a
#define LONG_OPT_HINT            0x13b
#define LONG_OPT_ACCTG_FREQ      0x13c
#define LONG_OPT_WCKEY           0x13d
#define LONG_OPT_RESERVATION     0x13e
#define LONG_OPT_SIGNAL          0x13f
#define LONG_OPT_TIME_MIN        0x140
#define LONG_OPT_GRES            0x141
#define LONG_OPT_WAIT_ALL_NODES  0x142
#define LONG_OPT_REQ_SWITCH      0x143
#define LONG_OPT_PROFILE         0x144
#define LONG_OPT_CPU_FREQ        0x145
#define LONG_OPT_GRES_FLAGS      0x146
#define LONG_OPT_SPREAD_JOB      0x147
#define LONG_OPT_PRIORITY        0x160
#define LONG_OPT_POWER           0x162
#define LONG_OPT_THREAD_SPEC     0x163
#define LONG_OPT_USE_MIN_NODES   0x164
#define LONG_OPT_MCS_LABEL       0x165
#define LONG_OPT_DEADLINE        0x166
#define LONG_OPT_DELAY_BOOT      0x167
#define LONG_OPT_CLUSTER_CONSTRAINT 0x168
#define LONG_OPT_X11             0x170
#define LONG_OPT_CPUS_PER_GPU    0x171
#define LONG_OPT_GPU_BIND        0x172
#define LONG_OPT_GPU_FREQ        0x173
#define LONG_OPT_GPUS            0x174
#define LONG_OPT_GPUS_PER_NODE   0x175
#define LONG_OPT_GPUS_PER_SOCKET 0x176
#define LONG_OPT_GPUS_PER_TASK   0x177
#define LONG_OPT_MEM_PER_GPU     0x178

/* THIS PROBABLY BELONGS IN slurm_opt.c */
struct slurm_long_option salloc_opts[] = {
	{
		.name		= "account",
		.get_func	= &arg_get_account,
		.set_func	= &arg_set_account,
		.has_arg	= required_argument,
		.opt_val	= 'A'
	}, {
		.name		= "extra-node-info",
		.get_func	= &arg_get_extra_node_info,
		.set_func	= &arg_set_extra_node_info,
		.has_arg	= required_argument,
		.opt_val	= 'B'
	}, {
		.name		= "cpus-per-task",
		.get_func	= &arg_get_cpus_per_task,
		.set_func	= &arg_set_cpus_per_task,
		.exit_on_error	= true,
		.has_arg	= required_argument,
		.opt_val	= 'c'
	},
	{"constraint",    &arg_get_constraint, 		&arg_set_constraint, false,
			  required_argument, 0, 'C'},
	{"cluster-constraint",&arg_get_cluster_constraint,&arg_set_cluster_constraint, false,
			  required_argument, 0, LONG_OPT_CLUSTER_CONSTRAINT},
	{"dependency",    &arg_get_dependency,		&arg_set_dependency, false,
			  required_argument, 0, 'd'},
	{"chdir",         &arg_get_chdir,		&arg_set_chdir, false,
			  required_argument, 0, 'D'},
	{"nodefile",      &arg_get_nodefile,		&arg_set_nodefile, true,
			  required_argument, 0, 'F'},
	{"gpus",          &arg_get_gpus,		&arg_set_gpus, false,
			  required_argument, 0, 'G'},
	{"help",          NULL,				NULL, true, /* DMJ set_func was _arg_help */
			  no_argument,       0, 'h'},
	{"hold",          &arg_get_hold,		&arg_set_hold, false,
			  no_argument,       0, 'H'},
	{"immediate",     &arg_get_immediate,		&arg_set_immediate, false,
			  optional_argument, 0, 'I'},
	{"job-name",      &arg_get_job_name,		&arg_set_job_name, false,
			  required_argument, 0, 'J'},
	{"no-kill",       &arg_get_no_kill,		&arg_set_no_kill, false,
			  no_argument,       0, 'k'},
	{"kill-command",  &arg_get_kill_command,	&arg_set_kill_command, false,
			  optional_argument, 0, 'K'},
	{"licenses",      &arg_get_licenses,		&arg_set_licenses, false,
			  required_argument, 0, 'L'},
	{"distribution",  &arg_get_distribution,	&arg_set_distribution, false,
			  required_argument, 0, 'm'},
	{"cluster",       &arg_get_cluster,		&arg_set_cluster, false,
			  required_argument, 0, 'M'},
	{"clusters",      &arg_get_cluster,		&arg_set_cluster, false,
			  required_argument, 0, 'M'},
	{"tasks",         &arg_get_ntasks,		&arg_set_ntasks, false,
			  required_argument, 0, 'n'},
	{"ntasks",        &arg_get_ntasks,		&arg_set_ntasks, false,
			  required_argument, 0, 'n'},
	{"nodes",         &arg_get_nodes,		&arg_set_nodes, true,
			  required_argument, 0, 'N'},
	{"overcommit",    &arg_get_overcommit,		&arg_set_overcommit, false,
			  no_argument,       0, 'O'},
	{"oversubscribe", &arg_get_oversubscribe,	&arg_set_oversubscribe, false,
			  no_argument,       0, 's'},
	{"partition",     &arg_get_partition,		&arg_set_partition, false,
			  required_argument, 0, 'p'},
	{"qos",		  &arg_get_qos,			&arg_set_qos, false,
			  required_argument, 0, 'q'},
	{"quiet",         &arg_get_quiet,		&arg_set_quiet, false,
			  no_argument,       0, 'Q'},
	{"share",         &arg_get_share,		&arg_set_share, false,
			  no_argument,       0, 's'},
	{"core-spec",     &arg_get_core_spec,		&arg_set_core_spec, false,
			  required_argument, 0, 'S'},
	{"time",          &arg_get_time,		&arg_set_time, true,
			  required_argument, 0, 't'},
	{"usage",         NULL,				NULL, true, /* DMJ set_func was _arg_usage */
			  no_argument,       0, 'u'},
	{"verbose",       &arg_get_verbose,		&arg_set_verbose, false,
			  no_argument,       0, 'v'},
	{"version",       NULL,				&arg_version, true,
			  no_argument,       0, 'V'},
	{"nodelist",      &arg_get_nodelist,		&arg_set_nodelist, false,
			  required_argument, 0, 'w'},
	{"wait",          &arg_get_wait,		&arg_set_wait, false,
			  required_argument, 0, 'W'},
	{"exclude",       &arg_get_exclude,		&arg_set_exclude, true,
			  required_argument, 0, 'x'},
	{"acctg-freq",    &arg_get_acctg_freq,		&arg_set_acctg_freq, false,
			  required_argument, 0, LONG_OPT_ACCTG_FREQ},
	{"begin",         &arg_get_begin,		&arg_set_begin, true,
			  required_argument, 0, LONG_OPT_BEGIN},
	{"bb",            &arg_get_bb,			&arg_set_bb, false,
			  required_argument, 0, LONG_OPT_BURST_BUFFER_SPEC},
	{"bbf",           &arg_get_bbf,			&arg_set_bbf, false,
			  required_argument, 0, LONG_OPT_BURST_BUFFER_FILE},
	{"bell",          &arg_get_bell,		&arg_set_bell, false,
			  no_argument,       0, LONG_OPT_BELL},
	{"comment",       &arg_get_comment,		&arg_set_comment, false,
			  required_argument, 0, LONG_OPT_COMMENT},
	{"contiguous",    &arg_get_contiguous,		&arg_set_contiguous, false,
			  no_argument,       0, LONG_OPT_CONT},
	{"cores-per-socket", &arg_get_cores_per_socket,	&arg_set_cores_per_socket, true,
			  required_argument, 0, LONG_OPT_CORESPERSOCKET},
	{"cpu-freq",      &arg_get_cpu_freq,		&arg_set_cpu_freq, false,
			  required_argument, 0, LONG_OPT_CPU_FREQ},
	{"cpus-per-gpu",  &arg_get_cpus_per_gpu,	&arg_set_cpus_per_gpu, true,
			  required_argument, 0, LONG_OPT_CPUS_PER_GPU},
	{"deadline",      &arg_get_deadline,		&arg_set_deadline, true,
			  required_argument, 0, LONG_OPT_DEADLINE},
	{"delay-boot",    &arg_get_delay_boot,		&arg_set_delay_boot, true,
			  required_argument, 0, LONG_OPT_DELAY_BOOT},
	{"exclusive",     &arg_get_exclusive,		&arg_set_exclusive, true,
			  optional_argument, 0, LONG_OPT_EXCLUSIVE},
	{"get-user-env",  &arg_get_get_user_env,	&arg_set_get_user_env, false,
			  optional_argument, 0, LONG_OPT_GET_USER_ENV},
	{"gid",           &arg_get_gid,			&arg_set_gid, true,
			  required_argument, 0, LONG_OPT_GID},
	{"gpu-bind",      &arg_get_gpu_bind,		&arg_set_gpu_bind, false,
			  required_argument, 0, LONG_OPT_GPU_BIND},
	{"gpu-freq",      &arg_get_gpu_freq,		&arg_set_gpu_freq, false,
			  required_argument, 0, LONG_OPT_GPU_FREQ},
	{"gpus-per-node", &arg_get_gpus_per_node,	&arg_set_gpus_per_node, false,
			  required_argument, 0, LONG_OPT_GPUS_PER_NODE},
	{"gpus-per-socket",&arg_get_gpus_per_socket,	&arg_set_gpus_per_socket, false,
			  required_argument, 0, LONG_OPT_GPUS_PER_SOCKET},
	{"gpus-per-task", &arg_get_gpus_per_task,	&arg_set_gpus_per_task, false,
			  required_argument, 0, LONG_OPT_GPUS_PER_TASK},
	{"gres",          &arg_get_gres,		&arg_set_gres, false,
			  required_argument, 0, LONG_OPT_GRES},
	{"gres-flags",    &arg_get_gres_flags,		&arg_set_gres_flags, true,
			  required_argument, 0, LONG_OPT_GRES_FLAGS},
	{"hint",          &arg_get_hint,		&arg_set_hint, false,
			  required_argument, 0, LONG_OPT_HINT},
	{"jobid",         &arg_get_jobid,		&arg_set_jobid, false,
			  required_argument, 0, LONG_OPT_JOBID},
	{"mail-type",     &arg_get_mail_type,		&arg_set_mail_type, true,
			  required_argument, 0, LONG_OPT_MAIL_TYPE},
	{"mail-user",     &arg_get_mail_user,		&arg_set_mail_user, false,
			  required_argument, 0, LONG_OPT_MAIL_USER},
	{"mcs-label",     &arg_get_mcs_label,		&arg_set_mcs_label, false,
			  required_argument, 0, LONG_OPT_MCS_LABEL},
	{"mem",           &arg_get_mem,			&arg_set_mem, true,
			  required_argument, 0, LONG_OPT_MEM},
	{"mem-per-cpu",   &arg_get_mem_per_cpu,		&arg_set_mem_per_cpu, true,
			  required_argument, 0, LONG_OPT_MEM_PER_CPU},
	{"mem-per-gpu",   &arg_get_mem_per_gpu,		&arg_set_mem_per_gpu, true,
			  required_argument, 0, LONG_OPT_MEM_PER_GPU},
	{"mem-bind",      &arg_get_mem_bind,		&arg_set_mem_bind, false,
			  required_argument, 0, LONG_OPT_MEM_BIND},
	{"mem_bind",      &arg_get_mem_bind,		&arg_set_mem_bind, false,
			  required_argument, 0, LONG_OPT_MEM_BIND},
	{"mincores",      &arg_get_mincores,		&arg_set_mincores, true,
			  required_argument, 0, LONG_OPT_MINCORES},
	{"mincpus",       &arg_get_mincpus,		&arg_set_mincpus, false,
			  required_argument, 0, LONG_OPT_MINCPU},
	{"minsockets",    &arg_get_minsockets,		&arg_set_minsockets, true,
			  required_argument, 0, LONG_OPT_MINSOCKETS},
	{"minthreads",    &arg_get_minthreads,		&arg_set_minthreads, true,
			  required_argument, 0, LONG_OPT_MINTHREADS},
	{"network",       &arg_get_network,		&arg_set_network, false,
			  required_argument, 0, LONG_OPT_NETWORK},
	{"nice",          &arg_get_nice,		&arg_set_nice, false,
			  optional_argument, 0, LONG_OPT_NICE},
	{"priority",      &arg_get_priority,		&arg_set_priority, true,
			  required_argument, 0, LONG_OPT_PRIORITY},
	{"no-bell",       &arg_get_no_bell,		&arg_set_no_bell, false,
			  no_argument,       0, LONG_OPT_NO_BELL},
	{"no-shell",      &arg_get_no_shell,		&arg_set_no_shell, false,
			  no_argument,       0, LONG_OPT_NOSHELL},
	{"ntasks-per-core",&arg_get_ntasks_per_core,	&arg_set_ntasks_per_core, false,
			  required_argument, 0, LONG_OPT_NTASKSPERCORE},
	{"ntasks-per-node",&arg_get_ntasks_per_node,	&arg_set_ntasks_per_node, false,
			  required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"ntasks-per-socket",&arg_get_ntasks_per_socket, &arg_set_ntasks_per_socket, false,
			  required_argument, 0, LONG_OPT_NTASKSPERSOCKET},
	{"power",         &arg_get_power,		&arg_set_power, false,
			  required_argument, 0, LONG_OPT_POWER},
	{"profile",       &arg_get_profile,		&arg_set_profile, false,
			  required_argument, 0, LONG_OPT_PROFILE},
	{"reboot",	  &arg_get_reboot,		&arg_set_reboot, false,
			  no_argument,       0, LONG_OPT_REBOOT},
	{"reservation",   &arg_get_reservation,		&arg_set_reservation, false,
			  required_argument, 0, LONG_OPT_RESERVATION},
	{"signal",        &arg_get_signal,		&arg_set_signal, true,
			  required_argument, 0, LONG_OPT_SIGNAL},
	{"sockets-per-node",&arg_get_sockets_per_node,	&arg_set_sockets_per_node, true,
			  required_argument, 0, LONG_OPT_SOCKETSPERNODE},
	{"spread-job",    &arg_get_spread_job,		&arg_set_spread_job, false,
			  no_argument,       0, LONG_OPT_SPREAD_JOB},
	{"switches",      &arg_get_switches,		&arg_set_switches, false,
			  required_argument, 0, LONG_OPT_REQ_SWITCH},
	{"tasks-per-node",&arg_get_tasks_per_node,	&arg_set_tasks_per_node, false,
			  required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"thread-spec",   &arg_get_thread_spec,		&arg_set_thread_spec, false,
			  required_argument, 0, LONG_OPT_THREAD_SPEC},
	{"time-min",      &arg_get_time_min,		&arg_set_time_min, true,
			  required_argument, 0, LONG_OPT_TIME_MIN},
	{"threads-per-core",&arg_get_threads_per_core,	&arg_set_threads_per_core, true,
			  required_argument, 0, LONG_OPT_THREADSPERCORE},
	{"tmp",           &arg_get_tmp,			&arg_set_tmp, true,
			  required_argument, 0, LONG_OPT_TMP},
	{"uid",           &arg_get_uid,			&arg_set_uid, true,
			  required_argument, 0, LONG_OPT_UID},
	{"use-min-nodes", &arg_get_use_min_nodes,	&arg_set_use_min_nodes, false,
			  no_argument,       0, LONG_OPT_USE_MIN_NODES},
	{"wait-all-nodes",&arg_get_wait_all_nodes,	&arg_set_wait_all_nodes, true,
			  required_argument, 0, LONG_OPT_WAIT_ALL_NODES},
	{"wckey",         &arg_get_wckey,		&arg_set_wckey, false,
			  required_argument, 0, LONG_OPT_WCKEY},
#ifdef WITH_SLURM_X11
	{"x11",           &arg_get_x11,			&arg_set_x11, false,
			  optional_argument, 0, LONG_OPT_X11},
#endif
	{NULL,            NULL,				NULL, false,
			  0,                 0, 0}
};
#endif

#include "src/common/optz.h"
extern struct option *option_table_create(struct slurm_long_option **base, int pass) {
	/* generate an option table from slurm_long_option.
 	 * this could have used the optz_* functions, but those appear to be
 	 * rather special purpose for spank operations
 	 */

	struct slurm_long_option **ptr;
	struct option *opts, *optr;
	int count = 0;

	/* how many options are there? */
	for (count = 0, ptr = base; ptr && *ptr; ptr++) {
		if ((*ptr)->pass == pass || (*ptr)->pass < 0)
			count++;
	}

	/* allocate and initialize table all at once *
	 * the +1 on the counts is to add a terminator to the table which is
 	 * initilized as a side effect (or the main point) of the memset
 	 */
	opts = xmalloc(sizeof(struct option) * (count + 1));
	memset(opts, 0, sizeof(struct option) * (count + 1));

	/* subset slurm_long_option into the option table */
	for (ptr = base, optr = opts; ptr && *ptr; ptr++, optr++) {
		if ((*ptr)->pass != pass && (*ptr)->pass >= 0)
			continue;
		optr->name = xstrdup((*ptr)->name);
		optr->has_arg = (*ptr)->has_arg;
		optr->val = (*ptr)->opt_val;

		/* flag may require better handling, but is not a used
 		 * feature in slurm, thus copying NULL is fine */
		optr->flag = (*ptr)->flag;
	}
	return opts;
}

extern void option_table_destroy(struct option *opts) {
	struct option *ptr;

	for (ptr = opts; ptr && ptr->name; ptr++) {
		xfree(ptr->name);
	}
	xfree(opts);
}

static char *_arg_gen_optstring(struct slurm_long_option **opt_table)
{
	char *opt_string = xstrdup("+");
	char suffix[3];
	struct slurm_long_option **ptr = NULL;

	for (ptr = opt_table; ptr && *ptr; ptr++) {
		if ((*ptr)->opt_val > 'z')
			continue;
		if (strchr(opt_string, (*ptr)->opt_val))
			continue;
		if ((*ptr)->has_arg == required_argument)
			snprintf(suffix, sizeof(suffix), ":");
		else if ((*ptr)->has_arg == optional_argument)
			snprintf(suffix, sizeof(suffix), "::");
		else
			suffix[0] = '\0';
		xstrfmtcat(opt_string, "%c%s", (*ptr)->opt_val, suffix);
	}
	return opt_string;
}

extern int arg_setoptions(slurm_opt_t *opt, int pass, int argc, char **argv)
{
	int opt_char, option_index = 0;
	struct slurm_long_option **opt_table, **optpptr;
	struct option *long_options = NULL;
	char *opt_string = NULL, *arg = NULL;
	struct option *optz = NULL;
	bool found;

	if (opt->srun_opt)
		opt_table = srun_options;
	else if (opt->salloc_opt)
		opt_table = salloc_options;
	else if (opt->sbatch_opt)
		opt_table = sbatch_options;
	else {
		error("Unable to identify executable to identify the option table.");
		exit(1);
	}
	long_options = option_table_create(opt_table, pass);
	opt_string = _arg_gen_optstring(opt_table);
	optz = spank_option_table_create(long_options);

	if (!optz) {
		error("Unable to create options table");
		exit(1);
	}

	opt->progname = xbasename(argv[0]);
	optind = 0;
	while ((opt_char = getopt_long(argc, argv, opt_string,
				      optz, &option_index)) != -1) {
		found = false;
		for (optpptr = opt_table; optpptr && *optpptr; optpptr++) {
			struct slurm_long_option *optptr = *optpptr;
			if (optptr->opt_val != opt_char)
				continue;
			arg = xstrdup_printf("--%s", optptr->name);
			(optptr->set_func)(opt, optarg, arg, optptr->exit_on_error);
			xfree(arg);
			found = true;
		}
		if (found)
			continue;
		
		if (opt_char == '?') {
			fprintf(stderr, "Try \"%s --help\" for more "
				"information\n", opt->progname);
			exit(1);
		}
		if (spank_process_option(opt_char, optarg) < 0)
			exit(1);
	}

	spank_option_table_destroy(optz);
	option_table_destroy(long_options);
	xfree(opt_string);
	return optind;
}

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

static uint16_t _parse_pbs_mail_type(const char *arg)
{
        uint16_t rc = 0;

        if (strchr(arg, 'b') || strchr(arg, 'B'))
                rc |= MAIL_JOB_BEGIN;
        if (strchr(arg, 'e') || strchr(arg, 'E'))
                rc |= MAIL_JOB_END;
        if (strchr(arg, 'a') || strchr(arg, 'A'))
                rc |= MAIL_JOB_FAIL;

        if (strchr(arg, 'n') || strchr(arg, 'N'))
                rc = 0;
        else if (!rc)
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
int get_signal_opts(const char *optarg, uint16_t *warn_signal, uint16_t *warn_time,
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
int sig_name2num(const char *signal_name)
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
	const char *ptr;
	char *eptr;
	long tmp;
	int i;

	tmp = strtol(signal_name, &eptr, 10);
	if (eptr != signal_name) { /* found a number */
		if (xstring_is_whitespace(eptr))
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

extern int arg_version(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	print_slurm_version();
	if (is_fatal) {
		exit(0);
	}
	return SLURM_SUCCESS;
}

extern int arg_help(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	struct slurm_long_option **opt_table = NULL, **ptr = NULL;
	char short_opt[6];
	char eqsign[3];
	char short_help[81];
	char short_help_term[2];
	if (opt->srun_opt)
		opt_table = srun_options;
	else if (opt->sbatch_opt)
		opt_table = sbatch_options;
	else if (opt->salloc_opt)
		opt_table = salloc_options;

	if (!opt_table) {
		error("Failed to identify which executable to display help for.");
		exit(1);
	}

	for (ptr = opt_table; ptr && *ptr; ptr++) {
		if ((*ptr)->opt_val < 0x100)
			snprintf(short_opt, sizeof(short_opt), "  -%c,",
				 (*ptr)->opt_val);
		else
			short_opt[0] = '\0';

		if ((*ptr)->has_arg == required_argument)
			snprintf(eqsign, sizeof(eqsign), "=");
		else if ((*ptr)->has_arg == optional_argument)
			snprintf(eqsign, sizeof(eqsign), "[=");
		else
			snprintf(eqsign, sizeof(eqsign), " ");

		if ((*ptr)->help_short)
			snprintf(short_help, sizeof(short_help), "%s", (*ptr)->help_short);
		else if ((*ptr)->has_arg != no_argument)
			snprintf(short_help, sizeof(short_help), "arg");
		else
			short_help[0] = '\0';

		if (eqsign[0] == '[')
			snprintf(short_help_term, sizeof(short_help_term), "]");
		else
			short_help_term[0] = '\0';

		printf("%5s --%s%s%s%s\n", short_opt, (*ptr)->name, eqsign, short_help, short_help_term);
	}
	exit(0);
	return SLURM_SUCCESS;
}

extern int arg_usage(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	printf("Usage displayed here!\n");
	return SLURM_SUCCESS;
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

static bool _valid_node_list(slurm_opt_t *opt, char **node_list_pptr)
{
        int count = NO_VAL;

        /* If we are using Arbitrary and we specified the number of
           procs to use then we need exactly this many since we are
           saying, lay it out this way!  Same for max and min nodes.
           Other than that just read in as many in the hostfile */
	if (opt->ntasks_set)
		count = opt->ntasks;
	else if (opt->nodes_set) {
		if (opt->max_nodes)
			count = opt->max_nodes;
		else if (opt->min_nodes)
			count = opt->min_nodes;
	}

	return verify_node_list(node_list_pptr, opt->distribution, count);
}

extern int arg_set_exclude(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->exc_nodes);
	opt->exc_nodes = xstrdup(arg);
	if (!_valid_node_list(opt, &opt->exc_nodes))
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

static void _proc_get_user_env(slurm_opt_t *opt, const char *optarg) {
	char *arg = xstrdup(optarg);
	char *end_ptr;
	if ((arg[0] >= '0') && (arg[0] <= '9'))
		opt->get_user_env_time = strtol(arg, &end_ptr, 10);
	else {
		opt->get_user_env_time = 0;
		end_ptr = arg;
	}

	if ((end_ptr == NULL) || (end_ptr[0] == '\0'))
		goto endit;
	if ((end_ptr[0] == 's') || (end_ptr[0] == 'S'))
		opt->get_user_env_mode = 1;
	else if ((end_ptr[0] == 'l') || (end_ptr[0] == 'L'))
		opt->get_user_env_mode = 2;

endit:
	xfree(arg);
}

extern int arg_set_get_user_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (opt->srun_opt) {
		error("--get-user-env is no longer supported in srun, use "
		      "sbatch");
		return SLURM_SUCCESS;
	}
	if (arg)
		_proc_get_user_env(opt, arg);
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
	xfree(opt->gpus_per_node);
	opt->gpus_per_node = xstrdup(arg);
	return SLURM_SUCCESS;
}

extern int arg_set_gpus_per_socket(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->gpus_per_socket);
	opt->gpus_per_socket = xstrdup(arg);
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
		cpu_bind_type = &sropt->cpu_bind_type;

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
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	if (sbopt)
		sbopt->ignore_pbs = 1;

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
	if (opt->srun_opt) {
		/* we expect this in srun, so no warning */
		opt->jobid_set = true;
		return SLURM_SUCCESS;
	}
	if (!opt->salloc_opt) {
		/* DMJ TODO this seems wrong, my guess is that salloc should also
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

extern int arg_set_pack_group(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	/* DMJ NOT IMPLEMENTED */
	return SLURM_SUCCESS;
}

extern int arg_set_pbsmail_type(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;

	opt->mail_type |= _parse_pbs_mail_type(arg);
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
	uint64_t mbytes = 0;
	if (!arg)
		return SLURM_ERROR;

	mbytes = str_to_mbytes2(arg);
	return arg_set_mem_mb(opt, mbytes, label, is_fatal);
}

extern int arg_set_mem_mb(slurm_opt_t *opt, uint64_t mbytes, const char *label, bool is_fatal) {

	opt->pn_min_memory = (int64_t) mbytes;
	if (opt->srun_opt) {
		/* only srun does this */
		opt->mem_per_cpu = NO_VAL64;
	}

	if (opt->pn_min_memory < 0)
		return _arg_set_error(label, "invalid memory constraint", "", is_fatal);
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
		return _arg_set_error(label, "invalid argument", arg);

	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_cpu_mb(slurm_opt_t *opt, int64_t mbytes) {
	opt->mem_per_cpu = mbytes;
	if (opt->srun_opt) {
		/* only srun does this */
		opt->pn_min_memory = NO_VAL64;
	}
	if (opt->mem_per_cpu < 0)
		return SLURM_ERROR;
	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_cpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int64_t mem_per_cpu = 0;
	if (!arg)
		return SLURM_ERROR;
	mem_per_cpu = (int64_t) str_to_mbytes2(arg);
	if (arg_set_mem_per_cpu_mb(opt, mem_per_cpu))
		return _arg_set_error(label, "invalid memory constraint", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_gpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	int64_t mbytes = 0;
	if (!arg)
		return SLURM_ERROR;
	mbytes = (int64_t) str_to_mbytes2(arg);
	if (arg_set_mem_per_gpu_mb(opt, mbytes))
		return _arg_set_error(label, "invalid mem-per-gpu constraint", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_mem_per_gpu_mb(slurm_opt_t *opt, int64_t mbytes) {
	opt->mem_per_gpu = mbytes;
	if (opt->mem_per_gpu < 0)
		return SLURM_ERROR;

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
		return _arg_set_error(label, "invalid argument", arg, is_fatal);

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
	struct utsname name;

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
		opt->no_kill = false;
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
	char *tmp = NULL;

	/* skip if srun */
	if (!arg)
		return SLURM_ERROR;
	if (opt->srun_opt)
		return SLURM_SUCCESS;

	xfree(opt->nodelist);
	tmp = slurm_read_hostfile(arg, 0);
	if (tmp) {
		opt->nodelist = xstrdup(tmp);
		/* DMJ TODO: this really should be free(), slurm_read_hostfile uses malloc(), maybe file a bug */
		free(tmp);
	} else
		return _arg_set_error(label, "invalid node file", arg, is_fatal);

	return SLURM_SUCCESS;
}

extern int arg_set_nodelist(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal) {
	if (!arg)
		return SLURM_ERROR;
	xfree(opt->nodelist);
	opt->nodelist = xstrdup(arg);

	if (!_valid_node_list(opt, &opt->nodelist))
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

extern int arg_set_ntasks_int(slurm_opt_t *opt, int ntasks, const char *label, bool is_fatal) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	opt->ntasks = ntasks;
	opt->ntasks_set = true;
	if (sbopt)
		sbopt->pack_env->ntasks = opt->ntasks;

	return SLURM_SUCCESS;
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
		return SLURM_ERROR;

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
	uint64_t mbytes = 0;
	if (!arg)
		return SLURM_ERROR;
	mbytes = str_to_mbytes2(arg);
	return arg_set_tmp_mb(opt, mbytes, label, is_fatal);
}

extern int arg_set_tmp_mb(slurm_opt_t *opt, uint64_t mbytes, const char *label, bool is_fatal) {
	opt->pn_min_tmp_disk = (int64_t) mbytes;
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
		return SLURM_ERROR;
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
	if ((sbopt->umask < 0) || (sbopt->umask > 0777)) {
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

extern char *arg_get_export(slurm_opt_t *opt) {
	sbatch_opt_t *sbopt = opt->sbatch_opt;
	srun_opt_t *sropt = opt->srun_opt;
	if (sbopt && sbopt->export_env)
		return xstrdup(sbopt->export_env);
	if (sropt && sropt->export_env)
		return xstrdup(sropt->export_env);
	return NULL;
}

extern char *arg_get_account(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_acctg_freq(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_bb(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_bbf(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_begin(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_bell(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_chdir(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cluster(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cluster_constraint(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_comment(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_constraint(slurm_opt_t *opt) {
	if (opt && opt->constraints)
		return xstrdup(opt->constraints);
	return NULL;
}

extern char *arg_get_contiguous(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_core_spec(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cores_per_socket(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cpu_freq(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cpus_per_gpu(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cpus_per_task(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_deadline(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_delay_boot(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_dependency(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_distribution(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_exclude(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_exclusive(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_extra_node_info(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_get_user_env(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gid(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpu_bind(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpu_freq(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpus(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpus_per_node(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpus_per_socket(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gpus_per_task(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_gres(slurm_opt_t *opt) {
	if (opt && opt->gres)
		return xstrdup(opt->gres);
	return NULL;
}

extern char *arg_get_gres_flags(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_hint(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_hold(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_immediate(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_jobid(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_job_name(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_kill_command(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_licenses(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mail_type(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mail_user(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mcs_label(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mem(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mem_bind(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mem_per_cpu(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mem_per_gpu(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mincores(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mincpus(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_minsockets(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_minthreads(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_network(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_nice(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_no_bell(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_nodefile(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_nodelist(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_nodes(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_no_kill(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_no_shell(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_ntasks(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_ntasks_per_core(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_ntasks_per_node(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_ntasks_per_socket(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_overcommit(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_oversubscribe(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_partition(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_power(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_priority(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_profile(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_qos(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_quiet(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_reboot(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_reservation(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_share(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_signal(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_sockets_per_node(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_spread_job(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_switches(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_tasks_per_node(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_thread_spec(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_threads(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_threads_per_core(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_time(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_time_min(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_tmp(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_uid(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_unbuffered(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_use_min_nodes(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_verbose(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_wait(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_wait_all_nodes(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_wckey(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_x11(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_array(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_batch(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_workdir(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_checkpoint(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_checkpoint_dir(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_clusters(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_error(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_export_file(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_ignore_pbs(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_input(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_kill_on_invalid_dep(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_no_requeue(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_open_mode(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_output(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_parsable(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_propagate(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_requeue(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_test_only(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_wrap(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_accel_bind(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_bcast(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_compress(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_cpu_bind(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_debugger_test(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_disable_status(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_epilog(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_join(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_kill_on_bad_exit(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_label(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_launch_cmd(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_launcher_opts(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_mpi(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_msg_timeout(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_multi_prog(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_no_allocate(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_pack_group(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_preserve_env(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_prolog(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_pty(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_quit_on_interrupt(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_relative(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_restart_dir(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_resv_ports(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_slurmd_debug(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_task_epilog(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_task_prolog(slurm_opt_t *opt)
{
	return NULL;
}

extern char *arg_get_tres_per_job(slurm_opt_t *opt)
{
	return NULL;
}
