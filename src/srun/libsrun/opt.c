/*****************************************************************************\
 *  opt.c - options processing for srun
 *****************************************************************************
 *  Copyright (C) 2002-2007 The Regents of the University of California.
 *  Copyright (C) 2008-2010 Lawrence Livermore National Security.
 *  Portions Copyright (C) 2010-2018 SchedMD LLC <https://www.schedmd.com>
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <grondona1@llnl.gov>, et. al.
 *  CODE-OCEC-09-009. All rights reserved.
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

#define _GNU_SOURCE

#include <ctype.h>		/* isdigit() */
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>		/* va_start   */
#include <stdio.h>
#include <stdlib.h>		/* getenv     */
#include <sys/param.h>		/* MAXPATHLEN */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "src/common/cpu_frequency.h"
#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/optz.h"
#include "src/common/parse_time.h"
#include "src/common/plugstack.h"
#include "src/common/proc_args.h"
#include "src/common/slurm_mpi.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_interface.h"
#include "src/common/slurm_rlimits_info.h"
#include "src/common/slurm_resource_info.h"
#include "src/common/slurm_acct_gather_profile.h"
#include "src/common/uid.h"
#include "src/common/x11_util.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/util-net.h"

#include "src/api/pmi_server.h"

#include "debugger.h"
#include "launch.h"
#include "multi_prog.h"
#include "opt.h"

/* generic OPT_ definitions -- mainly for use with env vars  */
#define OPT_NONE        0x00
#define OPT_INT         0x01
#define OPT_STRING      0x02
#define OPT_IMMEDIATE   0x03
#define OPT_DISTRIB     0x04
#define OPT_NODES       0x05
#define OPT_OVERCOMMIT  0x06
#define OPT_COMPRESS	0x07
#define OPT_RESV_PORTS	0x09
#define OPT_MPI         0x0c
#define OPT_CPU_BIND    0x0d
#define OPT_MEM_BIND    0x0e
#define OPT_MULTI       0x0f
#define OPT_NSOCKETS    0x10
#define OPT_NCORES      0x11
#define OPT_NTHREADS    0x12
#define OPT_EXCLUSIVE   0x13
#define OPT_OPEN_MODE   0x14
#define OPT_ACCTG_FREQ  0x15
#define OPT_WCKEY       0x16
#define OPT_SIGNAL      0x17
#define OPT_TIME_VAL    0x18
#define OPT_CPU_FREQ    0x19
#define OPT_CORE_SPEC   0x1a
#define OPT_GRES_FLAGS	0x1b
#define OPT_POWER       0x1c
#define OPT_THREAD_SPEC 0x1d
#define OPT_BCAST       0x1e
#define OPT_PROFILE     0x20
#define OPT_EXPORT	0x21
#define OPT_HINT	0x22
#define OPT_SPREAD_JOB  0x23
#define OPT_DELAY_BOOT  0x24
#define OPT_INT64	0x25
#define OPT_USE_MIN_NODES 0x26
#define OPT_MEM_PER_GPU   0x27
#define OPT_NO_KILL       0x28

extern char **environ;

/*---- global variables, defined in opt.h ----*/
int	error_exit = 1;
int	immediate_exit = 1;
slurm_opt_t opt;
srun_opt_t sropt;
List 	opt_list = NULL;
int	pass_number = 0;
time_t	srun_begin_time = 0;
bool	tres_bind_err_log = true;
bool	tres_freq_err_log = true;
int	_verbose = 0;

/*---- forward declarations of static variables and functions  ----*/
typedef struct env_vars env_vars_t;
struct option long_options[] = {
	{"account",          required_argument, 0, 'A'},
	{"extra-node-info",  required_argument, 0, 'B'},
	{"cpus-per-task",    required_argument, 0, 'c'},
	{"constraint",       required_argument, 0, 'C'},
	{"cluster-constraint",required_argument,0, LONG_OPT_CLUSTER_CONSTRAINT},
	{"dependency",       required_argument, 0, 'd'},
	{"chdir",            required_argument, 0, 'D'},
	{"error",            required_argument, 0, 'e'},
	{"preserve-env",     no_argument,       0, 'E'},
	{"preserve-slurm-env", no_argument,     0, 'E'},
	{"gpus",             required_argument, 0, 'G'},
	{"hold",             no_argument,       0, 'H'},
	{"input",            required_argument, 0, 'i'},
	{"immediate",        optional_argument, 0, 'I'},
	{"join",             no_argument,       0, 'j'},
	{"job-name",         required_argument, 0, 'J'},
	{"no-kill",          optional_argument, 0, 'k'},
	{"kill-on-bad-exit", optional_argument, 0, 'K'},
	{"label",            no_argument,       0, 'l'},
	{"licenses",         required_argument, 0, 'L'},
	{"cluster",          required_argument, 0, 'M'},
	{"clusters",         required_argument, 0, 'M'},
	{"distribution",     required_argument, 0, 'm'},
	{"ntasks",           required_argument, 0, 'n'},
	{"nodes",            required_argument, 0, 'N'},
	{"output",           required_argument, 0, 'o'},
	{"overcommit",       no_argument,       0, 'O'},
	{"oversubscribe",    no_argument,       0, 's'},
	{"partition",        required_argument, 0, 'p'},
	{"qos",		     required_argument, 0, 'q'},
	{"quiet",            no_argument,       0, 'Q'},
	{"relative",         required_argument, 0, 'r'},
	{"share",            no_argument,       0, 's'},
	{"core-spec",        required_argument, 0, 'S'},
	{"time",             required_argument, 0, 't'},
	{"threads",          required_argument, 0, 'T'},
	{"unbuffered",       no_argument,       0, 'u'},
	{"verbose",          no_argument,       0, 'v'},
	{"version",          no_argument,       0, 'V'},
	{"nodelist",         required_argument, 0, 'w'},
	{"wait",             required_argument, 0, 'W'},
	{"exclude",          required_argument, 0, 'x'},
	{"disable-status",   no_argument,       0, 'X'},
	{"no-allocate",      no_argument,       0, 'Z'},
	{"accel-bind",       required_argument, 0, LONG_OPT_ACCEL_BIND},
	{"acctg-freq",       required_argument, 0, LONG_OPT_ACCTG_FREQ},
	{"bb",               required_argument, 0, LONG_OPT_BURST_BUFFER_SPEC},
	{"bbf",              required_argument, 0, LONG_OPT_BURST_BUFFER_FILE},
	{"bcast",            optional_argument, 0, LONG_OPT_BCAST},
	{"begin",            required_argument, 0, LONG_OPT_BEGIN},
	{"checkpoint",       required_argument, 0, LONG_OPT_CHECKPOINT},
	{"checkpoint-dir",   required_argument, 0, LONG_OPT_CHECKPOINT_DIR},
	{"compress",         optional_argument, 0, LONG_OPT_COMPRESS},
	{"comment",          required_argument, 0, LONG_OPT_COMMENT},
	{"contiguous",       no_argument,       0, LONG_OPT_CONT},
	{"cores-per-socket", required_argument, 0, LONG_OPT_CORESPERSOCKET},
	{"cpu-bind",         required_argument, 0, LONG_OPT_CPU_BIND},
	{"cpu_bind",         required_argument, 0, LONG_OPT_CPU_BIND},
	{"cpu-freq",         required_argument, 0, LONG_OPT_CPU_FREQ},
	{"cpus-per-gpu",     required_argument, 0, LONG_OPT_CPUS_PER_GPU},
	{"deadline",         required_argument, 0, LONG_OPT_DEADLINE},
	{"debugger-test",    no_argument,       0, LONG_OPT_DEBUG_TS},
	{"delay-boot",       required_argument, 0, LONG_OPT_DELAY_BOOT},
	{"epilog",           required_argument, 0, LONG_OPT_EPILOG},
	{"exclusive",        optional_argument, 0, LONG_OPT_EXCLUSIVE},
	{"export",           required_argument, 0, LONG_OPT_EXPORT},
	{"get-user-env",     optional_argument, 0, LONG_OPT_GET_USER_ENV},
	{"gid",              required_argument, 0, LONG_OPT_GID},
	{"gpu-bind",         required_argument, 0, LONG_OPT_GPU_BIND},
	{"gpu-freq",         required_argument, 0, LONG_OPT_GPU_FREQ},
	{"gpus-per-node",    required_argument, 0, LONG_OPT_GPUS_PER_NODE},
	{"gpus-per-socket",  required_argument, 0, LONG_OPT_GPUS_PER_SOCKET},
	{"gpus-per-task",    required_argument, 0, LONG_OPT_GPUS_PER_TASK},
	{"gres",             required_argument, 0, LONG_OPT_GRES},
	{"gres-flags",       required_argument, 0, LONG_OPT_GRES_FLAGS},
	{"help",             no_argument,       0, LONG_OPT_HELP},
	{"hint",             required_argument, 0, LONG_OPT_HINT},
	{"jobid",            required_argument, 0, LONG_OPT_JOBID},
	{"launch-cmd",       no_argument,       0, LONG_OPT_LAUNCH_CMD},
	{"launcher-opts",    required_argument, 0, LONG_OPT_LAUNCHER_OPTS},
	{"mail-type",        required_argument, 0, LONG_OPT_MAIL_TYPE},
	{"mail-user",        required_argument, 0, LONG_OPT_MAIL_USER},
	{"mcs-label",        required_argument, 0, LONG_OPT_MCS_LABEL},
	{"mem",              required_argument, 0, LONG_OPT_MEM},
	{"mem-per-cpu",      required_argument, 0, LONG_OPT_MEM_PER_CPU},
	{"mem-per-gpu",      required_argument, 0, LONG_OPT_MEM_PER_GPU},
	{"mem-bind",         required_argument, 0, LONG_OPT_MEM_BIND},
	{"mem_bind",         required_argument, 0, LONG_OPT_MEM_BIND},
	{"mincores",         required_argument, 0, LONG_OPT_MINCORES},
	{"mincpus",          required_argument, 0, LONG_OPT_MINCPUS},
	{"minsockets",       required_argument, 0, LONG_OPT_MINSOCKETS},
	{"minthreads",       required_argument, 0, LONG_OPT_MINTHREADS},
	{"mpi",              required_argument, 0, LONG_OPT_MPI},
	{"msg-timeout",      required_argument, 0, LONG_OPT_TIMEO},
	{"multi-prog",       no_argument,       0, LONG_OPT_MULTI},
	{"network",          required_argument, 0, LONG_OPT_NETWORK},
	{"nice",             optional_argument, 0, LONG_OPT_NICE},
	{"ntasks-per-core",  required_argument, 0, LONG_OPT_NTASKSPERCORE},
	{"ntasks-per-node",  required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"ntasks-per-socket",required_argument, 0, LONG_OPT_NTASKSPERSOCKET},
	{"open-mode",        required_argument, 0, LONG_OPT_OPEN_MODE},
	{"pack-group",       required_argument, 0, LONG_OPT_PACK_GROUP},
	{"power",            required_argument, 0, LONG_OPT_POWER},
	{"priority",         required_argument, 0, LONG_OPT_PRIORITY},
	{"profile",          required_argument, 0, LONG_OPT_PROFILE},
	{"prolog",           required_argument, 0, LONG_OPT_PROLOG},
	{"propagate",        optional_argument, 0, LONG_OPT_PROPAGATE},
	{"pty",              no_argument,       0, LONG_OPT_PTY},
	{"quit-on-interrupt",no_argument,       0, LONG_OPT_QUIT_ON_INTR},
	{"reboot",           no_argument,       0, LONG_OPT_REBOOT},
	{"reservation",      required_argument, 0, LONG_OPT_RESERVATION},
	{"restart-dir",      required_argument, 0, LONG_OPT_RESTART_DIR},
	{"resv-ports",       optional_argument, 0, LONG_OPT_RESV_PORTS},
	{"signal",	     required_argument, 0, LONG_OPT_SIGNAL},
	{"slurmd-debug",     required_argument, 0, LONG_OPT_DEBUG_SLURMD},
	{"sockets-per-node", required_argument, 0, LONG_OPT_SOCKETSPERNODE},
	{"spread-job",       no_argument,       0, LONG_OPT_SPREAD_JOB},
	{"switches",         required_argument, 0, LONG_OPT_REQ_SWITCH},
	{"task-epilog",      required_argument, 0, LONG_OPT_TASK_EPILOG},
	{"task-prolog",      required_argument, 0, LONG_OPT_TASK_PROLOG},
	{"tasks-per-node",   required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"test-only",        no_argument,       0, LONG_OPT_TEST_ONLY},
	{"thread-spec",      required_argument, 0, LONG_OPT_THREAD_SPEC},
	{"time-min",         required_argument, 0, LONG_OPT_TIME_MIN},
	{"threads-per-core", required_argument, 0, LONG_OPT_THREADSPERCORE},
	{"tres-per-job",     required_argument, 0, LONG_OPT_TRES_PER_JOB},
	{"tmp",              required_argument, 0, LONG_OPT_TMP},
	{"uid",              required_argument, 0, LONG_OPT_UID},
	{"use-min-nodes",    no_argument,       0, LONG_OPT_USE_MIN_NODES},
	{"usage",            no_argument,       0, LONG_OPT_USAGE},
	{"wckey",            required_argument, 0, LONG_OPT_WCKEY},
#ifdef WITH_SLURM_X11
	{"x11",              optional_argument, 0, LONG_OPT_X11},
#endif
	{NULL,               0,                 0, 0}
	};
char *opt_string = "+A:B:c:C:d:D:e:EG:hHi:I::jJ:k::K::lL:m:M:n:N:"
		   "o:Op:P:q:Qr:sS:t:T:uU:vVw:W:x:XZ";


static slurm_opt_t *_get_first_opt(int pack_offset);
static slurm_opt_t *_get_next_opt(int pack_offset, slurm_opt_t *opt_last);

static int  _get_task_count(void);

/* Get a decimal integer from arg */
static int  _get_int(const char *arg, const char *what, bool positive);

static bitstr_t *_get_pack_group(const int argc, char **argv,
				 int default_pack_offset, bool *opt_found);

static void  _help(void);

/* fill in default options  */
static void _opt_default(void);

/* set options based upon env vars  */
static void _opt_env(int pack_offset, int pass);

static void _opt_args(int argc, char **argv, int pack_offset);

/* list known options and their settings  */
static void  _opt_list(void);

/* verify options sanity  */
static bool _opt_verify(void);

static void  _process_env_var(env_vars_t *e, const char *val);
static char *_read_file(char *fname);
static void  _set_options(const int argc, char **argv);
static bool  _under_parallel_debugger(void);
static void  _usage(void);
static bool  _valid_node_list(char **node_list_pptr);

/*---[ end forward declarations of static functions ]---------------------*/

/*
 * Find first option structure for a given pack job offset
 * pack_offset IN - Offset into pack job or -1 if regular job
 * RET - Pointer to option structure or NULL if none found
 */
static slurm_opt_t *_get_first_opt(int pack_offset)
{
	ListIterator opt_iter;
	slurm_opt_t *opt_local;

	if (!opt_list) {
		if (!sropt.pack_grp_bits && (pack_offset == -1))
			return &opt;
		if (sropt.pack_grp_bits &&
		    (pack_offset >= 0) &&
		    (pack_offset < bit_size(sropt.pack_grp_bits)) &&
		    bit_test(sropt.pack_grp_bits, pack_offset))
			return &opt;
		return NULL;
	}

	opt_iter = list_iterator_create(opt_list);
	while ((opt_local = list_next(opt_iter))) {
		srun_opt_t *srun_opt = opt_local->srun_opt;
		xassert(srun_opt);
		if (srun_opt->pack_grp_bits && (pack_offset >= 0)
		    && (pack_offset < bit_size(srun_opt->pack_grp_bits))
		    && bit_test(srun_opt->pack_grp_bits, pack_offset))
			break;
	}
	list_iterator_destroy(opt_iter);

	return opt_local;
}

/*
 * Find next option structure for a given pack job offset
 * pack_offset IN - Offset into pack job or -1 if regular job
 * opt_last IN - past option structure found for this pack offset
 * RET - Pointer to option structure or NULL if none found
 */
static slurm_opt_t *_get_next_opt(int pack_offset, slurm_opt_t *opt_last)
{
	ListIterator opt_iter;
	slurm_opt_t *opt_local;
	bool found_last = false;

	if (!opt_list)
		return NULL;

	opt_iter = list_iterator_create(opt_list);
	while ((opt_local = list_next(opt_iter))) {
		srun_opt_t *srun_opt = opt_local->srun_opt;
		xassert(srun_opt);
		if (!found_last) {
			if (opt_last == opt_local)
				found_last = true;
			continue;
		}

		if (srun_opt->pack_grp_bits && (pack_offset >= 0)
		    && (pack_offset < bit_size(srun_opt->pack_grp_bits))
		    && bit_test(srun_opt->pack_grp_bits, pack_offset))
			break;
	}
	list_iterator_destroy(opt_iter);

	return opt_local;
}

/*
 * Find option structure for a given pack job offset
 * pack_offset IN - Offset into pack job, -1 if regular job, -2 to reset
 * RET - Pointer to next matching option structure or NULL if none found
 */
extern slurm_opt_t *get_next_opt(int pack_offset)
{
	static int offset_last = -2;
	static slurm_opt_t *opt_last = NULL;

	if (pack_offset == -2) {
		offset_last = -2;
		opt_last = NULL;
		return NULL;
	}

	if (offset_last != pack_offset) {
		offset_last = pack_offset;
		opt_last = _get_first_opt(pack_offset);
	} else {
		opt_last = _get_next_opt(pack_offset, opt_last);
	}
	return opt_last;
}

/*
 * Return maximum pack_group value for any step launch option request
 */
extern int get_max_pack_group(void)
{
	ListIterator opt_iter;
	slurm_opt_t *opt_local;
	int max_pack_offset = 0, pack_offset = 0;

	if (opt_list) {
		opt_iter = list_iterator_create(opt_list);
		while ((opt_local = list_next(opt_iter))) {
			srun_opt_t *srun_opt = opt_local->srun_opt;
			xassert(srun_opt);
			if (srun_opt->pack_grp_bits)
				pack_offset = bit_fls(srun_opt->pack_grp_bits);
			if (pack_offset >= max_pack_offset)
				max_pack_offset = pack_offset;
		}
		list_iterator_destroy(opt_iter);
	} else {
		if (sropt.pack_grp_bits)
			max_pack_offset = bit_fls(sropt.pack_grp_bits);
	}

	return max_pack_offset;
}

/*
 * Copy the last option record:
 * Copy strings if the original values will be preserved and
 *   reused for additional heterogeneous job/steps
 * Otherwise clear/NULL the pointer so it does not get re-used
 *   and freed, which will render the copied pointer bad
 */
static slurm_opt_t *_opt_copy(void)
{
	slurm_opt_t *opt_dup;
	int i;

	opt_dup = xmalloc(sizeof(slurm_opt_t));
	memcpy(opt_dup, &opt, sizeof(slurm_opt_t));
	opt_dup->srun_opt = xmalloc(sizeof(srun_opt_t));
	memcpy(opt_dup->srun_opt, &sropt, sizeof(srun_opt_t));

	opt_dup->account = xstrdup(opt.account);
	opt_dup->acctg_freq = xstrdup(opt.acctg_freq);
	sropt.alloc_nodelist = NULL;	/* Moved by memcpy */
	opt_dup->srun_opt->argv = xmalloc(sizeof(char *) * sropt.argc);
	for (i = 0; i < sropt.argc; i++)
		opt_dup->srun_opt->argv[i] = xstrdup(sropt.argv[i]);
	sropt.bcast_file = NULL;	/* Moved by memcpy */
	opt.burst_buffer = NULL;	/* Moved by memcpy */
	opt_dup->c_constraints = xstrdup(opt.c_constraints);
	opt_dup->srun_opt->ckpt_dir = xstrdup(sropt.ckpt_dir);
	opt_dup->srun_opt->ckpt_interval_str =
		xstrdup(sropt.ckpt_interval_str);
	opt_dup->clusters = xstrdup(opt.clusters);
	opt_dup->srun_opt->cmd_name = xstrdup(sropt.cmd_name);
	opt_dup->comment = xstrdup(opt.comment);
	opt.constraints = NULL;		/* Moved by memcpy */
	opt_dup->srun_opt->cpu_bind = xstrdup(sropt.cpu_bind);
	opt_dup->cwd = xstrdup(opt.cwd);
	opt_dup->dependency = xstrdup(opt.dependency);
	opt_dup->srun_opt->efname = xstrdup(sropt.efname);
	opt_dup->srun_opt->epilog = xstrdup(sropt.epilog);
	opt_dup->exc_nodes = xstrdup(opt.exc_nodes);
	opt_dup->srun_opt->export_env = xstrdup(sropt.export_env);
	opt_dup->extra = xstrdup(opt.extra);
	opt.gres = NULL;		/* Moved by memcpy */
	opt_dup->gpus = xstrdup(opt.gpus);
	opt_dup->gpu_bind = xstrdup(opt.gpu_bind);
	opt_dup->gpu_freq = xstrdup(opt.gpu_freq);
	opt_dup->gpus_per_node = xstrdup(opt.gpus_per_node);
	opt_dup->gpus_per_socket = xstrdup(opt.gpus_per_socket);
	opt_dup->gpus_per_task = xstrdup(opt.gpus_per_task);
	opt.hint_env = NULL;		/* Moved by memcpy */
	sropt.hostfile = NULL;		/* Moved by memcpy */
	opt_dup->srun_opt->ifname = xstrdup(sropt.ifname);
	opt_dup->job_name = xstrdup(opt.job_name);
	opt_dup->srun_opt->ofname = xstrdup(sropt.ofname);
	opt_dup->srun_opt->launcher_opts = xstrdup(sropt.launcher_opts);
	sropt.launcher_opts = NULL;	/* Moved by memcpy */
	opt.licenses = NULL;		/* Moved by memcpy */
	opt.mail_user = NULL;		/* Moved by memcpy */
	opt_dup->mcs_label = xstrdup(opt.mcs_label);
	opt.mem_bind = NULL;		/* Moved by memcpy */
	opt_dup->mpi_type = xstrdup(opt.mpi_type);
	opt.network = NULL;		/* Moved by memcpy */
	opt.nodelist = NULL;		/* Moved by memcpy */
	sropt.pack_group = NULL;	/* Moved by memcpy */
	sropt.pack_grp_bits = NULL;	/* Moved by memcpy */
	opt.partition = NULL;		/* Moved by memcpy */
	/* NOTE: Do NOT copy "progname", shared by all job components */
	opt_dup->srun_opt->prolog = xstrdup(sropt.prolog);
	opt_dup->srun_opt->propagate = xstrdup(sropt.propagate);
	opt_dup->qos = xstrdup(opt.qos);
	opt_dup->reservation = xstrdup(opt.reservation);
	sropt.restart_dir = NULL;	/* Moved by memcpy */
	opt.spank_job_env = NULL;	/* Moved by memcpy */
	opt_dup->srun_opt->task_epilog = xstrdup(sropt.task_epilog);
	opt_dup->srun_opt->task_prolog = xstrdup(sropt.task_prolog);
	opt_dup->time_limit_str = xstrdup(opt.time_limit_str);
	opt_dup->time_min_str = xstrdup(opt.time_min_str);
	opt_dup->tres_bind = xstrdup(opt.tres_bind);
	opt_dup->tres_freq = xstrdup(opt.tres_freq);
	opt_dup->user = xstrdup(opt.user);
	opt_dup->wckey = xstrdup(opt.wckey);

	return opt_dup;
}

/*
 * process options:
 * 1. set defaults
 * 2. update options with env vars
 * 3. update options with commandline args
 * 4. perform some verification that options are reasonable
 *
 * argc IN - Count of elements in argv
 * argv IN - Array of elements to parse
 * argc_off OUT - Offset of first non-parsable element
 */
extern int initialize_and_process_args(int argc, char **argv, int *argc_off)
{
	static int default_pack_offset = 0;
	static bool pending_append = false;
	bitstr_t *pack_grp_bits;
	int i, i_first, i_last;
	bool opt_found = false;

	pack_grp_bits = _get_pack_group(argc, argv, default_pack_offset++,
					&opt_found);
	i_first = bit_ffs(pack_grp_bits);
	i_last  = bit_fls(pack_grp_bits);
	for (i = i_first; i <= i_last; i++) {
		if (!bit_test(pack_grp_bits, i))
			continue;
		pass_number++;
		if (pending_append) {
			if (!opt_list)
				opt_list = list_create(NULL);
			list_append(opt_list, _opt_copy());
			pending_append = false;
		}

		/* initialize option defaults */
		_opt_default();
		if (opt_found || (i > 0)) {
			xstrfmtcat(sropt.pack_group, "%d", i);
			sropt.pack_grp_bits = bit_alloc(MAX_PACK_COUNT);
			bit_set(sropt.pack_grp_bits, i);
		}

		/* initialize options with env vars */
		_opt_env(i, 0);

		/* initialize options with argv */
		_set_options(argc, argv);
		_opt_args(argc, argv, i);

		if (argc_off)
			*argc_off = optind;

		_opt_env(i, 1);

		if (!_opt_verify())
			exit(error_exit);

		if (opt.verbose)
			_opt_list();

		if (sropt.launch_cmd) {
			char *launch_type = slurm_get_launch_type();
			if (!xstrcmp(launch_type, "launch/slurm")) {
				error("--launch-cmd option is invalid with %s",
				      launch_type);
				xfree(launch_type);
				exit(1);
			}
			xfree(launch_type);
			/* Massage ntasks value earlier than normal */
			if (!opt.ntasks_set)
				opt.ntasks = _get_task_count();
			launch_g_create_job_step(NULL, 0, NULL, NULL, &opt);
			exit(0);
		}
		if (spank_init_post_opt() < 0) {
			error("Plugin stack post-option processing failed.");
			exit(error_exit);
		}
		pending_append = true;
	}
	bit_free(pack_grp_bits);

	if (opt_list && pending_append) {		/* Last record */
		list_append(opt_list, _opt_copy());
		pending_append = false;
	}

	return 1;
}

static int _get_task_count(void)
{
	char *cpus_per_node = NULL, *end_ptr = NULL;
	int cpu_count, node_count, task_count, total_tasks = 0;

	if (opt.ntasks_per_node != NO_VAL)
		return (opt.min_nodes * opt.ntasks_per_node);
	if (opt.cpus_set)
		cpus_per_node = getenv("SLURM_JOB_CPUS_PER_NODE");
	if (cpus_per_node) {
		cpu_count = strtol(cpus_per_node, &end_ptr, 10);
		task_count = cpu_count / opt.cpus_per_task;
		while (1) {
			if ((end_ptr[0] == '(') && (end_ptr[1] == 'x')) {
				end_ptr += 2;
				node_count = strtol(end_ptr, &end_ptr, 10);
				task_count *= node_count;
				total_tasks += task_count;
				if (end_ptr[0] == ')')
					end_ptr++;
			} else if ((end_ptr[0] == ',') || (end_ptr[0] == 0))
				total_tasks += task_count;
			else {
				error("Invalid value for environment variable "
				      "SLURM_JOB_CPUS_PER_NODE (%s)",
				      cpus_per_node);
				break;
			}
			if (end_ptr[0] == ',')
				end_ptr++;
			if (end_ptr[0] == 0)
				break;
		}
		return total_tasks;
	}
	return opt.min_nodes;
}

/*
 * If the node list supplied is a file name, translate that into
 *	a list of nodes, we orphan the data pointed to
 * RET true if the node list is a valid one
 */
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

/*
 * print error message to stderr with opt.progname prepended
 */
#undef USE_ARGERROR
#if USE_ARGERROR
static void argerror(const char *msg, ...)
  __attribute__ ((format (printf, 1, 2)));
static void argerror(const char *msg, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);

	fprintf(stderr, "%s: %s\n",
		opt.progname ? opt.progname : "srun", buf);
	va_end(ap);
}
#else
#  define argerror error
#endif				/* USE_ARGERROR */

/*
 * _opt_default(): used by initialize_and_process_args to set defaults
 */
static void _opt_default(void)
{
	char *launch_params;
	char buf[MAXPATHLEN + 1];
	uid_t uid = getuid();

	if (pass_number == 1) {
		opt.salloc_opt = NULL;
		opt.sbatch_opt = NULL;
		opt.srun_opt = &sropt;
		xfree(opt.account);
		xfree(opt.acctg_freq);
		sropt.allocate		= false;
		opt.begin		= (time_t) 0;
		xfree(opt.c_constraints);
		xfree(sropt.ckpt_dir);
		sropt.ckpt_dir			= slurm_get_checkpoint_dir();
		sropt.ckpt_interval		= 0;
		xfree(sropt.ckpt_interval_str);
		xfree(opt.clusters);
		xfree(sropt.cmd_name);
		xfree(opt.comment);
		opt.cpus_per_gpu	= 0;
		if ((getcwd(buf, MAXPATHLEN)) == NULL) {
			error("getcwd failed: %m");
			exit(error_exit);
		}
		opt.cwd			= xstrdup(buf);
		sropt.cwd_set		= false;
		opt.deadline		= 0;
		sropt.debugger_test	= false;
		opt.delay_boot		= NO_VAL;
		xfree(opt.dependency);
		sropt.disable_status	= false;
		opt.distribution	= SLURM_DIST_UNKNOWN;
		opt.egid		= (gid_t) -1;
		xfree(sropt.efname);
		xfree(sropt.epilog);
		sropt.epilog		= slurm_get_srun_epilog();
		xfree(opt.extra);
		xfree(opt.exc_nodes);
		xfree(sropt.export_env);
		opt.euid		= (uid_t) -1;
		opt.gid			= getgid();
		xfree(opt.gpus);
		xfree(opt.gpu_bind);
		xfree(opt.gpu_freq);
		xfree(opt.gpus_per_node);
		xfree(opt.gpus_per_socket);
		xfree(opt.gpus_per_task);
		opt.hold		= false;
		xfree(sropt.ifname);
		opt.immediate		= 0;
		opt.jobid		= NO_VAL;
		opt.jobid_set		= false;
		xfree(opt.job_name);
		sropt.job_name_set_cmd	= false;
		sropt.job_name_set_env	= false;
		sropt.kill_bad_exit	= NO_VAL;
		sropt.labelio		= false;
		sropt.max_wait		= slurm_get_wait_time();
		xfree(opt.mcs_label);
		opt.mem_per_gpu		= 0;
		/* Default launch msg timeout           */
		sropt.msg_timeout		= slurm_get_msg_timeout();
		opt.nice		= NO_VAL;
		opt.no_kill		= false;
		sropt.no_alloc		= false;
		sropt.noshell		= false;
		xfree(sropt.ofname);
		sropt.open_mode		= 0;
		sropt.parallel_debug	= false;
		sropt.pty			= false;
		sropt.preserve_env	= false;
		opt.priority		= 0;
		opt.profile		= ACCT_GATHER_PROFILE_NOT_SET;
		xfree(opt.progname);
		xfree(sropt.prolog);
		sropt.prolog		= slurm_get_srun_prolog();
		xfree(sropt.propagate); 	 /* propagate specific rlimits */
		sropt.quit_on_intr	= false;
		xfree(opt.qos);
		opt.quiet		= 0;
		opt.reboot		= false;
		xfree(opt.reservation);
		sropt.slurmd_debug	= LOG_LEVEL_QUIET;
		xfree(sropt.task_epilog);
		xfree(sropt.task_prolog);
		sropt.test_only		= false;
		sropt.test_exec		= false;
		opt.time_limit		= NO_VAL;
		xfree(opt.time_limit_str);
		opt.time_min		= NO_VAL;
		xfree(opt.time_min_str);
		xfree(opt.tres_per_job);
		opt.uid			= uid;
		sropt.unbuffered	= false;
		opt.user		= uid_to_string(uid);
		sropt.user_managed_io	= false;
		if (xstrcmp(opt.user, "nobody") == 0)
			fatal("Invalid user id: %u", uid);
		opt.warn_flags		= 0;
		opt.warn_signal		= 0;
		opt.warn_time		= 0;
		xfree(opt.wckey);
		opt.verbose		= 0;
	}

	/*
	 * All other options must be specified individually for each component
	 * of the job/step. Do not use xfree() as the pointers have been copied.
	 * See initialize_and_process_args() above.
	 */
	sropt.alloc_nodelist		= NULL;
	sropt.accel_bind_type		= 0;
	sropt.bcast_file		= NULL;
	sropt.bcast_flag		= false;
	sropt.accel_bind_type		= 0;
	opt.burst_buffer		= NULL;
	sropt.compress			= 0;
	opt.constraints			= NULL;
	opt.contiguous			= false;
	opt.core_spec			= NO_VAL16;
	sropt.core_spec_set		= false;
	opt.cores_per_socket		= NO_VAL; /* requested cores */
	sropt.cpu_bind			= NULL;
	sropt.cpu_bind_type		= 0;
	sropt.cpu_bind_type_set		= false;
	opt.cpu_freq_min		= NO_VAL;
	opt.cpu_freq_max		= NO_VAL;
	opt.cpu_freq_gov		= NO_VAL;
	opt.cpus_per_task		= 0;
	opt.cpus_set			= false;
	sropt.exclusive			= false;
	opt.extra_set			= false;
	opt.gres			= NULL;
	opt.hint_env			= NULL;
	opt.hint_set			= false;
	sropt.hostfile			= NULL;
	sropt.exclusive			= false;
	opt.job_flags			= 0;
	sropt.launch_cmd		= false;
	sropt.launcher_opts		= NULL;
	launch_params = slurm_get_launch_params();
	if (launch_params && strstr(launch_params, "mem_sort"))
		opt.mem_bind_type	|= MEM_BIND_SORT;
	xfree(launch_params);
	opt.licenses			= NULL;
	opt.mail_type			= 0;
	opt.mail_user			= NULL;
	sropt.max_threads		= MAX_THREADS;
	pmi_server_max_threads(sropt.max_threads);
	opt.max_nodes			= 0;
	opt.mem_bind			= NULL;
	opt.mem_bind_type		= 0;
	opt.mem_per_cpu			= NO_VAL64;
	opt.min_nodes			= 1;
	sropt.multi_prog			= false;
	sropt.multi_prog_cmds		= 0;
	opt.network			= NULL;
	sropt.network_set_env		= false;
	opt.nodelist			= NULL;
	opt.nodes_set			= false;
	sropt.nodes_set_env		= false;
	sropt.nodes_set_opt		= false;
	opt.ntasks			= 1;
	opt.ntasks_per_core		= NO_VAL;
	opt.ntasks_per_core_set 	= false;
	opt.ntasks_per_node		= NO_VAL; /* ntask max limits */
	opt.ntasks_per_socket		= NO_VAL;
	opt.ntasks_set			= false;
	opt.overcommit			= false;
	sropt.pack_group		= NULL;
	sropt.pack_grp_bits		= NULL;
	opt.partition			= NULL;
	opt.plane_size			= NO_VAL;
	opt.pn_min_cpus			= NO_VAL;
	opt.pn_min_memory		= NO_VAL64;
	opt.pn_min_tmp_disk		= NO_VAL;
	opt.power_flags			= 0;
	sropt.relative			= NO_VAL;
	sropt.relative_set		= false;
	opt.req_switch			= -1;
	sropt.resv_port_cnt		= NO_VAL;
	sropt.restart_dir		= NULL;
	opt.shared			= NO_VAL16;
	opt.sockets_per_node		= NO_VAL; /* requested sockets */
	opt.spank_job_env_size		= 0;
	opt.spank_job_env		= NULL;
	opt.threads_per_core		= NO_VAL; /* requested threads */
	opt.threads_per_core_set	= false;
	opt.wait4switch			= -1;

	/*
	 * Reset some default values if running under a parallel debugger
	 */
	if ((sropt.parallel_debug = _under_parallel_debugger())) {
		sropt.max_threads		= 1;
		pmi_server_max_threads(sropt.max_threads);
		sropt.msg_timeout		= 15;
	}
}

/*---[ env var processing ]-----------------------------------------------*/

/*
 * try to use a similar scheme as popt.
 *
 * in order to add a new env var (to be processed like an option):
 *
 * define a new entry into env_vars[], if the option is a simple int
 * or string you may be able to get away with adding a pointer to the
 * option to set. Otherwise, process var based on "type" in _opt_env.
 */
struct env_vars {
        const char *var;
        int *(set_func)(slurm_opt_t *, const char *, const char *, bool);
        int eval_pass;
        int exit_on_error;
};

env_vars_t env_vars[] = {
{"SLURMD_DEBUG",        &arg_set_verbose,		0,	0 },
{"SLURM_ACCOUNT",       &arg_set_account,		0,	0 },
{"SLURM_ACCTG_FREQ",    &arg_set_acctg_freq,		0,	0 },
{"SLURM_BCAST",         &arg_set_bcast,			0,	0 },
{"SLURM_BURST_BUFFER",  &arg_set_bb,			0,	0 },
{"SLURM_CLUSTERS",      &arg_set_clusters,		0,	0 },
{"SLURM_CHECKPOINT",    &arg_set_checkpoint,		0,	1 },
{"SLURM_CHECKPOINT_DIR",&arg_set_checkpoint_dir,	0,	0 },
{"SLURM_COMPRESS",      &arg_set_compress,		0,	0 },
{"SLURM_CONSTRAINT",    &arg_set_constraint,		0,	0 },
{"SLURM_CLUSTER_CONSTRAINT",&arg_set_cluster_constraint,0,	0 },
{"SLURM_CORE_SPEC",     &arg_set_core_spec,		0,	0 },
{"SLURM_CPUS_PER_GPU",  &arg_set_cpus_per_gpu,		0,	0 },
{"SLURM_CPUS_PER_TASK", &arg_set_cpus_per_task,		0,	1 },
{"SLURM_CPU_BIND",      &arg_set_cpu_bind,		0,	1 },
{"SLURM_CPU_FREQ_REQ",  &arg_set_cpu_freq,		0,	0 },
{"SLURM_DELAY_BOOT",    &arg_set_delay_boot,		0,	0 },
{"SLURM_DEPENDENCY",    &arg_set_dependency,		0,	0 },
{"SLURM_DISABLE_STATUS",&arg_set_disable_status,	0,	0 },
{"SLURM_DISTRIBUTION",  &arg_set_distribution,		0,	0 },
{"SLURM_EPILOG",        &arg_set_epilog,		0,	0 },
{"SLURM_EXCLUSIVE",     &arg_set_exclusive,		0,	0 },
{"SLURM_EXPORT_ENV",    &arg_set_export_env,		0,	0 },
{"SLURM_GRES",          &arg_set_gres,			0,	0 },
{"SLURM_GRES_FLAGS",    &arg_set_gres_flags,		0,	1 },
{"SLURM_GPUS",          &arg_set_gpus,			0,	0 },
{"SLURM_GPU_BIND",      &arg_set_gpu_bind,		0,	0 },
{"SLURM_GPU_FREQ",      &arg_set_gpu_freq,		0,	0 },
{"SLURM_GPUS_PER_NODE", &arg_set_gpus_per_node,		0,	0 },
{"SLURM_GPUS_PER_SOCKET",&arg_set_gpus_per_socket,	0,	0 },
{"SLURM_GPUS_PER_TASK", &arg_set_gpus_per_task,		0,	0 },
{"SLURM_HINT",          &arg_set_hint,			1,	1 },
{"SLURM_IMMEDIATE",     &arg_set_immediate,		0,	0 },
/* SLURM_JOBID was used in slurm version 1.3 and below, it is now vestigial */
{"SLURM_JOBID",         &arg_set_job_id,		0,	0 },
{"SLURM_JOB_ID",        &arg_set_job_id,		0,	0 },
{"SLURM_JOB_NAME",      &arg_set_job_name_fromenv,	0,	0 },
{"SLURM_KILL_BAD_EXIT", &arg_set_kill_bad_exit,		0,	0 },
{"SLURM_LABELIO",       &arg_set_labelio,		0,	0 },
{"SLURM_MEM_BIND",      &arg_set_mem_bind,		0,	0 },
{"SLURM_MEM_PER_CPU",	&arg_set_mem_per_cpu,		0,	0 },
{"SLURM_MEM_PER_GPU",   &arg_set_mem_per_gpu,		0,	0},
{"SLURM_MEM_PER_NODE",	&arg_set_mem_per_node,		0,	0 },
{"SLURM_MPI_TYPE",      &arg_set_mpi_type,		0,	0 },
{"SLURM_NCORES_PER_SOCKET",&arg_set_ncores_per_socket,	0,	0 },
{"SLURM_NETWORK",       &arg_set_network_fromenv,	0,	0 },
{"SLURM_JOB_NUM_NODES", &arg_set_nodes_fromenv,		0,	0 },
{"SLURM_JOB_NODELIST",  &arg_set_job_nodelist,		0,	0 },
{"SLURM_NTASKS",        &arg_set_ntasks,		0,	0 },
{"SLURM_NPROCS",        &arg_set_nprocs,		0,	0 },
{"SLURM_NSOCKETS_PER_NODE",&arg_set_nsockets_per_node,	0,	0 },
{"SLURM_NTASKS_PER_NODE", &arg_set_ntasks_per_node,	0,	0 },
{"SLURM_NTHREADS_PER_CORE",&arg_set_nthreads_per_core,	0,	0 },
{"SLURM_NO_KILL",	&arg_set_no_kill,		0,	0 },
{"SLURM_OPEN_MODE",     &arg_set_open_mode,		0,	0 },
{"SLURM_OVERCOMMIT",    &arg_set_overcommit,		0,	0 },
{"SLURM_PARTITION",     &arg_set_partition,		0,	0 },
{"SLURM_POWER",         &arg_set_power,			0,	0 },
{"SLURM_PROFILE",       &arg_set_profile,		0,	0 },
{"SLURM_PROLOG",        &arg_set_prolog,		0,	0 },
{"SLURM_QOS",           &arg_set_qos,			0,	0 },
{"SLURM_REMOTE_CWD",    &arg_set_remote_cwd,		0,	0 },
{"SLURM_REQ_SWITCH",    &arg_setcomp_req_switch,	0,	0 },
{"SLURM_RESERVATION",   &arg_set_reservation,		0,	0 },
{"SLURM_RESTART_DIR",   &arg_set_restart_dir,		0,	0 },
{"SLURM_RESV_PORTS",    &arg_set_resv_ports,		0,	0 },
{"SLURM_SPREAD_JOB",    &arg_set_spread_job,		0,	0 },
{"SLURM_SIGNAL",        &arg_set_signal,		0,	1 },
{"SLURM_SRUN_MULTI",    &arg_set_srun_multi,		0,	0 },
{"SLURM_STDERRMODE",    &arg_set_stderrmode,		0,	0 },
{"SLURM_STDINMODE",     &arg_set_stdinmode,		0,	0 },
{"SLURM_STDOUTMODE",    &arg_set_stdoutmode,		0,	0 },
{"SLURM_TASK_EPILOG",   &arg_set_task_epilog,		0,	0 },
{"SLURM_TASK_PROLOG",   &arg_set_task_prolog,		0,	0 },
{"SLURM_THREAD_SPEC",   &arg_set_thread_spec,		0,	0 },
{"SLURM_THREADS",       &arg_set_threads,		0,	0 },
{"SLURM_TIMELIMIT",     &arg_set_time,			0,	1 },
{"SLURM_UNBUFFEREDIO",  &arg_set_unbufferedio,		0,	0 },
{"SLURM_USE_MIN_NODES", &arg_set_use_min_nodes,		0,	0 },
{"SLURM_WAIT",          &arg_set_wait,			0,	0 },
{"SLURM_WAIT4SWITCH",   &arg_setcomp_wait4switch,	0,	0 },
{"SLURM_WCKEY",         &arg_set_wckey,			0,	0 },
{"SLURM_WORKING_DIR",   &arg_set_working_dir,		0,	0 },
{NULL, NULL, 0, 0}
};


/*
 * _opt_env(): used by initialize_and_process_args to set options via
 *            environment variables. See comments above for how to
 *            extend srun to process different vars
 */
static void _opt_env(int pack_offset, int pass)
{
	char       key[64], *val = NULL;
	env_vars_t *e   = env_vars;

	while (e->var) {
		if (e->eval_pass != pass)
			continue;
		if ((val = getenv(e->var)))
			(e->set_func)(&opt, val, e->var, (bool) e->exit_on_error);
		if ((pack_offset >= 0) &&
		    strcmp(e->var, "SLURM_JOBID") &&
		    strcmp(e->var, "SLURM_JOB_ID")) {
			snprintf(key, sizeof(key), "%s_PACK_GROUP_%d",
				 e->var, pack_offset);
			if ((val = getenv(key)))
				(e->set_func)(&opt, val, key, (bool) e->exit_on_error);
		}
		e++;
	}

	/* Running srun within an existing srun. Don't inherit values. */
	if (getenv("SLURM_STEP_ID")) {
		xfree(sropt.cpu_bind);
		sropt.cpu_bind_type = 0;
		xfree(opt.mem_bind);
		opt.mem_bind_type = 0;
	}

	/* Process spank env options */
	if (spank_process_env_options())
		exit(error_exit);
}

/*
 *  Get a decimal integer from arg.
 *
 *  Returns the integer on success, exits program on failure.
 *
 */
static int
_get_int(const char *arg, const char *what, bool positive)
{
	return parse_int(what, arg, positive);
}

/*
 * If --pack-group option found, return a bitmap representing their IDs
 * argc IN - Argument count
 * argv IN - Arguments
 * default_pack_offset IN - Default offset
 * opt_found OUT - Set to true if --pack-group option found
 * RET bitmap if pack groups to run
 */
/* DMJ TODO -- THIS NEEDS TO GET INTEGRATED INTO THE ARG* work */
static bitstr_t *_get_pack_group(const int argc, char **argv,
				 int default_pack_offset, bool *opt_found)
{
	int i, opt_char, option_index = 0;
	char *tmp = NULL;
	bitstr_t *pack_grp_bits = bit_alloc(MAX_PACK_COUNT);
	hostlist_t hl;
	struct option *optz;

	optz = spank_option_table_create(long_options);
	if (!optz) {
		error("Unable to create option table");
		exit(error_exit);
	}

	*opt_found = false;
	optind = 0;
	while ((opt_char = getopt_long(argc, argv, opt_string,
				       optz, &option_index)) != -1) {
		switch (opt_char) {
		case LONG_OPT_PACK_GROUP:
			xfree(sropt.pack_group);
			sropt.pack_group = xstrdup(optarg);
			*opt_found = true;
		}
	}
	spank_option_table_destroy(optz);

	if (*opt_found == false) {
		bit_set(pack_grp_bits, default_pack_offset);
		return pack_grp_bits;
	}

	if (sropt.pack_group[0] == '[')
		tmp = xstrdup(sropt.pack_group);
	else
		xstrfmtcat(tmp, "[%s]", sropt.pack_group);
	hl = hostlist_create(tmp);
	if (!hl) {
		error("Invalid --pack-group value: %s", sropt.pack_group);
		exit(error_exit);
	}
	xfree(tmp);

	while ((tmp = hostlist_shift(hl))) {
		char *end_ptr = NULL;
		i = strtol(tmp, &end_ptr, 10);
		if ((i < 0) || (i >= MAX_PACK_COUNT) || (end_ptr[0] != '\0')) {
			error("Invalid --pack-group value: %s",
			       sropt.pack_group);
			exit(error_exit);
		}
		bit_set(pack_grp_bits, i);
		free(tmp);
	}
	hostlist_destroy(hl);
	if (bit_ffs(pack_grp_bits) == -1) {	/* No bits set */
		error("Invalid --pack-group value: %s", sropt.pack_group);
		exit(error_exit);
	}

	return pack_grp_bits;
}

static void _set_options(const int argc, char **argv)
{
	int opt_char, option_index = 0, max_val = 0, tmp_int;
	struct utsname name;
	char *pos_delimit;
	bool ntasks_set_opt = false;
	bool nodes_set_opt = false;

#ifdef HAVE_PTY_H
	char *tmp_str;
#endif
	struct option *optz = spank_option_table_create (long_options);

	if (!optz) {
		error("Unable to create option table");
		exit(error_exit);
	}

	if (opt.progname == NULL)
		opt.progname = xbasename(argv[0]);
	else if (pass_number <= 1)
		error("opt.progname is already set.");
	optind = 0;
	while ((opt_char = getopt_long(argc, argv, opt_string,
				       optz, &option_index)) != -1) {
		switch (opt_char) {
		case (int)'?':
			fprintf(stderr,
				"Try \"srun --help\" for more information\n");
			exit(error_exit);
			break;
		case (int)'A':
		case (int)'U':	/* backwards compatibility */
			arg_set_account(&opt, optarg, "--account", false);
			break;
		case (int)'B':
			arg_set_extra_node_info(&opt, optarg,
						"--extra-node-info", true);
			break;
		case (int)'c':
			arg_set_cpus_per_task(&opt, optarg, "--cpus-per-task",
					      true);
			break;
		case (int)'C':
			arg_set_constraint(&opt, optarg, "--constraint", false);
			break;
		case (int)'d':
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case (int)'D':
			arg_set_chdir(&opt, optarg, "--chdir", false);
			break;
		case (int)'e':
			arg_set_error(&opt, optarg, "--error", true);
			break;
		case (int)'E':
			arg_set_preserve_env(&opt, NULL, "--preserv-env", false);
			break;
		case (int)'G':
			arg_set_gpus(&opt, optarg, "--gpus", false);
			break;
		case (int)'H':
			arg_set_hold(&opt, NULL, "--hold", false);
			break;
		case (int)'i':
			arg_set_input(&opt, optarg, "--input", true);
			break;
		case (int)'I':
			arg_set_immediate(&opt, optarg, "--immediate", false);
			break;
		case (int)'j':
			/* Vestigial option */
			arg_set_join(&opt, NULL, "--join", false);
			break;
		case (int)'J':
			arg_set_job_name(&opt, optarg, "--job-name", false);
			break;
		case (int)'k':
			arg_set_no_kill(&opt, optarg, "--no-kill", false);
			break;
		case (int)'K':
			arg_set_kill_on_bad_exit(&opt, optarg, "--kill-on-bad-exit", false);
			break;
		case (int)'l':
			arg_set_label(&opt, NULL, "--label", false);
			break;
		case 'L':
			arg_set_licenses(&opt, optarg, "--licenses", false);
			break;
		case 'M':
			arg_set_clusters(&opt, optarg, "--clusters", false);
			break;
		case (int)'m':
			arg_set_distribution(&opt, optarg, "--distribution",
					     true);
			break;
		case (int)'n':
			arg_set_ntasks(&opt, optarg, "--ntasks", false);
			break;
		case (int)'N':
			arg_set_nodes(&opt, optarg, "--nodes", true);
			break;
		case (int)'o':
			arg_set_output(&opt, optarg, "--output", true);
			break;
		case (int)'O':
			arg_set_overcommit(&opt, NULL, "--overcommit", false);
			break;
		case (int)'p':
			arg_set_partition(&opt, optarg)
			break;
		case (int)'P':
			verbose("-P option is deprecated, use -d instead");
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case (int)'q':
			arg_set_qos(&opt, optarg, "--qos", false);
			break;
		case (int)'Q':
			arg_set_quiet(&opt, NULL, "--quiet", false);
			break;
		case (int)'r':
			arg_set_relative(&opt, optarg, "--relative", false);
			break;
		case (int)'s':
			arg_set_share(&opt, NULL, "--share", false);
			break;
		case (int)'S':
			arg_set_core_spec(&opt, optarg, "--core-spec", false);
			break;
		case (int)'t':
			arg_set_time(&opt, optarg, "--time", true);
			break;
		case (int)'T':
			arg_set_threads(&opt, optarg, "--threads", false);
			break;
		case (int)'u':
			arg_set_unbuffered(&opt, NULL, "--unbuffered", false);
			break;
		case (int)'v':
			arg_set_verbose(&opt, NULL, "--verbose", false);
			break;
		case (int)'V':
			print_slurm_version();
			exit(0);
			break;
		case (int)'w':
			arg_set_nodelist(&opt, optarg, "--nodelist", false);
			break;
		case (int)'W':
			arg_set_wait(&opt, optarg, "--wait", false);
			break;
		case (int)'x':
			arg_set_exclude(&opt, optarg, "--exclude", true);
			break;
		case (int)'X':
			arg_set_disable_status(&opt, NULL, "--disable-status", false);
			break;
		case (int)'Z':
			arg_set_no_allocate(&opt, NULL, "--no-allocate", false);
			break;
		case LONG_OPT_CLUSTER_CONSTRAINT:
			arg_set_cluster_constraint(&opt, optarg, "--cluster-constraint", false);
			break;
		case LONG_OPT_CONT:
			arg_set_contiguous(&opt, NULL, "--contiguous", false);
			break;
		case LONG_OPT_CPUS_PER_GPU:
			arg_set_cpus_per_gpu(&opt, optarg, "--cpus-per-gpu", true);
			break;
		case LONG_OPT_DEADLINE:
			arg_set_deadline(&opt, optarg, "--deadline", true);
			break;
		case LONG_OPT_EXCLUSIVE:
			arg_set_exclusive(&opt, optarg, "--exclusive", true);
			break;
		case LONG_OPT_EXPORT:
			arg_set_export(&opt, optarg, "--export", false);
			break;
		case LONG_OPT_BCAST:
			arg_set_bcast(&opt, optarg, "bcast", false);
			break;
		case LONG_OPT_CPU_BIND:
			arg_set_cpu_bind(&opt, optarg, "--cpu-bind", true);
			break;
		case LONG_OPT_LAUNCH_CMD:
			arg_set_launch_cmd(&opt, NULL, "--launch-cmd", false);
			break;
		case LONG_OPT_GPU_BIND:
			arg_set_gpu_bind(&opt, optarg, "--gpu-bind", false);
			break;
		case LONG_OPT_GPU_FREQ:
			arg_set_gpu_freq(&opt, optarg, "--gpu-freq", false);
			break;
		case LONG_OPT_GPUS_PER_NODE:
			arg_set_gpus_per_node(&opt, optarg, "--gpus-per-node", false);
			break;
		case LONG_OPT_GPUS_PER_SOCKET:
			arg_set_gpus_per_socket(&opt, optarg, "--gpus-per-socket", false);
			break;
		case LONG_OPT_GPUS_PER_TASK:
			arg_set_gpus_per_task(&opt, optarg, "--gpus-per-task", false);
			break;
		case LONG_OPT_MEM_PER_GPU:
			arg_set_mem_per_gpu(&opt, optarg, "--mem-per-gpu", true);
			break;
		case LONG_OPT_MEM_BIND:
			arg_set_mem_bind(&opt, optarg, "--mem-bind", true);
			break;
		case LONG_OPT_MINCPUS:
			arg_set_mincpus(&opt, optarg, "--mincpus", false);
			break;
		case LONG_OPT_MINCORES:
			arg_set_mincores(&opt, optarg, "--mincores", true);
			break;
		case LONG_OPT_MINSOCKETS:
			arg_set_minsockets(&opt, optarg, "--minsockets", true);
			break;
		case LONG_OPT_MINTHREADS:
			arg_set_minthreads(&opt, optarg, "--minthreads", true);
			break;
		case LONG_OPT_MEM:
			arg_set_mem(&opt, optarg, "--mem", true);
			break;
		case LONG_OPT_MEM_PER_CPU:
			arg_set_mem_per_cpu(&opt, optarg, "--mem-per-cpu", true);
			break;
		case LONG_OPT_MPI:
			arg_set_mpi(&opt, optarg, "--mpi", false);
			break;
		case LONG_OPT_PACK_GROUP:
			/* Already parsed in _get_pack_group() */
			break;
		case LONG_OPT_RESV_PORTS:
			arg_set_resv_ports(&opt, optarg, "--resv-ports", false);
			break;
		case LONG_OPT_TMP:
			arg_set_tmp(&opt, optarg, "--tmp", true);
			break;
		case LONG_OPT_JOBID:
			arg_set_jobid(&opt, optarg, "--jobid", false);
			break;
		case LONG_OPT_TIMEO:
			arg_set_msg_timeout(&opt, optarg, "--msg-timeout", false);
			break;
		case LONG_OPT_UID:
			arg_set_uid(&opt, optarg, "--uid", true);
			break;
		case LONG_OPT_GID:
			arg_set_gid(&opt, optarg, "--gid", true);
			break;
		case LONG_OPT_DEBUG_SLURMD:
			arg_set_slurmd_debug(&opt, optarg, "--debug-slurmd", false);
			break;
		case LONG_OPT_DEBUG_TS:
			arg_set_debugger_test(&opt, NULL, "--debugger-test", false);
			break;
		case 'h':
		case LONG_OPT_HELP:
			_help();
			exit(0);
		case LONG_OPT_USAGE:
			_usage();
			exit(0);
		case LONG_OPT_TEST_ONLY:
			arg_set_test_only(&opt, NULL, "--test-only", false);
			break;
		case LONG_OPT_NETWORK:
			arg_set_network(&opt, optarg, "--network", false);
			break;
		case LONG_OPT_PROPAGATE:
			arg_set_propagate(&opt, optarg, "--propagate", false);
			break;
		case LONG_OPT_PROLOG:
			arg_set_prolog(&opt, optarg, "--prolog", false);
			break;
		case LONG_OPT_EPILOG:
			arg_set_epilog(&opt, optarg, "--epilog", false);
			break;
		case LONG_OPT_BURST_BUFFER_SPEC:
			arg_set_bb(&opt, optarg, "--bb", false);
			break;
		case LONG_OPT_BURST_BUFFER_FILE:
			arg_set_bbf(&opt, optarg, "--bbf", false);
			break;
		case LONG_OPT_BEGIN:
			arg_set_begin(&opt, optarg, "--begin", true);
			break;
		case LONG_OPT_MAIL_TYPE:
			arg_set_mail_type(&opt, optarg, "--mail-type", true);
			break;
		case LONG_OPT_MAIL_USER:
			arg_set_mail_user(&opt, optarg, "--mail-user", false);
			break;
		case LONG_OPT_MCS_LABEL:
			arg_set_mcs_label(&opt, optarg, "--mcs-label", false);
			break;
		case LONG_OPT_TASK_PROLOG:
			arg_set_task_prolog(&opt, optarg, "--task-prolog", false);
			break;
		case LONG_OPT_TASK_EPILOG:
			arg_set_task_epilog(&opt, optarg, "--task-epilog", false);
			break;
		case LONG_OPT_NICE:
			arg_set_nice(&opt, optarg, "--nice", false);
			break;
		case LONG_OPT_PRIORITY:
			arg_set_priority(&opt, optarg, "--priority", true);
			break;
		case LONG_OPT_MULTI:
			arg_set_multi_prog(&opt, NULL, "--multi-prog", false);
			break;
		case LONG_OPT_COMMENT:
			arg_set_comment(&opt, optarg, "--comment", false);
			break;
		case LONG_OPT_SOCKETSPERNODE:
			arg_set_sockets_per_node(&opt, optarg, "--sockets-per-node", false);
			break;
		case LONG_OPT_CORESPERSOCKET:
			arg_set_cores_per_socket(&opt, optarg, "--cores-per-socket", false);
			break;
		case LONG_OPT_THREADSPERCORE:
			arg_set_threads_per_core(&opt, optarg, "--threads-per-core", false);
			break;
		case LONG_OPT_NTASKSPERNODE:
			arg_set_ntasks_per_node(&opt, optarg, "--ntasks-per-node", false);
			break;
		case LONG_OPT_NTASKSPERSOCKET:
			arg_set_ntasks_per_socket(&opt, optarg, "--ntasks-per-socket", false);
			break;
		case LONG_OPT_NTASKSPERCORE:
			arg_set_ntasks_per_core(&opt, optarg, "--ntasks-per-core", false);
			break;
		case LONG_OPT_HINT:
			arg_set_hint(&opt, optarg, "--hint", true);
			break;
		case LONG_OPT_REBOOT:
			arg_set_reboot(&opt, NULL, "--reboot", false);
			break;
		case LONG_OPT_GET_USER_ENV:
			error("--get-user-env is no longer supported in srun, "
			      "use sbatch");
			break;
		case LONG_OPT_PTY:
			arg_set_pty(&opt, NULL, "--pty", true);
			break;
		case LONG_OPT_CHECKPOINT:
			arg_set_checkpoint(&opt, optarg, "--checkpoint", true);
			break;
		case LONG_OPT_OPEN_MODE:
			arg_set_open_mode(&opt, optarg, "--open-mode", false);
			break;
		case LONG_OPT_ACCTG_FREQ:
			arg_set_acctg_freq(&opt, optarg, "--acctg-freq", true);
			break;
		case LONG_OPT_WCKEY:
			arg_set_wckey(&opt, optarg, "--wckey", false);
			break;
		case LONG_OPT_PROFILE:
			arg_set_profile(&opt, optarg, "--profile", false);
			break;
		case LONG_OPT_RESERVATION:
			arg_set_reservation(&opt, optarg, "--reservation", false);
			break;
		case LONG_OPT_LAUNCHER_OPTS:
			arg_set_launcher_opts(&opt, optarg, "--launcher-opts", false);
			break;
		case LONG_OPT_CHECKPOINT_DIR:
			arg_set_checkpoint_dir(&opt, optarg, "--checkpoint-dir", false);
			break;
		case LONG_OPT_RESTART_DIR:
			arg_set_restart_dir(&opt, optarg, "--restart-dir", false);
			break;
		case LONG_OPT_SIGNAL:
			arg_set_signal(&opt, optarg, "--signal", true);
			break;
		case LONG_OPT_TIME_MIN:
			arg_set_time_min(&opt, optarg, "--time-min", true);
			break;
		case LONG_OPT_TRES_PER_JOB:
			/*
			 * Currently undocumented option.
			 * For future use and testing cons_tres.
			 */
			xfree(opt.tres_per_job);
			opt.tres_per_job = xstrdup(optarg);
			break;
		case LONG_OPT_GRES:
			arg_set_gres(&opt, optarg, "--gres", false);
			break;
		case LONG_OPT_GRES_FLAGS:
			arg_set_gres_flags(&opt, optarg, "--gres-flags", true);
			break;
		case LONG_OPT_CPU_FREQ:
			arg_set_cpu_freq(&opt, optarg, "--cpu-freq", false);
			break;
		case LONG_OPT_REQ_SWITCH:
			arg_set_switches(&opt, optarg, "--switches", false);
			break;
		case LONG_OPT_POWER:
			arg_set_power(&opt, optarg, "--power", false);
			break;
		case LONG_OPT_THREAD_SPEC:
			arg_set_thread_spec(&opt, optarg, "--thread-spec", false);
			break;
		case LONG_OPT_ACCEL_BIND:
			arg_set_accel_bind(&opt, optarg, "--accel-bind", false);
			break;
		case LONG_OPT_COMPRESS:
			arg_set_compress(&opt, optarg, "--compress", false);
			break;
		case LONG_OPT_SPREAD_JOB:
			arg_set_spread_job(&opt, NULL, "--spread-job", false);
			break;
		case LONG_OPT_DELAY_BOOT:
			arg_set_delay_boot(&opt, optarg, "--delay-boot", true);
			break;
		case LONG_OPT_USE_MIN_NODES:
			arg_set_use_min_nodes(&opt, NULL, "--use-min-nodes", false);
			break;
		case LONG_OPT_QUIT_ON_INTR:
			arg_set_quit_on_interrupt(&opt, NULL, "--quit-on-interrupt", false);
			break;
#ifdef WITH_SLURM_X11
		case LONG_OPT_X11:
			arg_set_x11(&opt, optarg, "--x11", false);
			break;
#endif
		default:
			if (spank_process_option (opt_char, optarg) < 0)
				exit(error_exit);
		}
	}

	/* This means --ntasks was read from the environment.  We will override
	 * it with what the user specified in the hostlist. POE launched
	 * jobs excluded (they have the SLURM_STARTED_STEP env var set). */
	if (((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_ARBITRARY)
	    && !getenv("SLURM_STARTED_STEP")) {
		if (!ntasks_set_opt)
			opt.ntasks_set = false;
		if (!nodes_set_opt)
			opt.nodes_set = false;
	}

	spank_option_table_destroy (optz);
}

/*
 * _opt_args() : set options via commandline args and popt
 */
static void _opt_args(int argc, char **argv, int pack_offset)
{
	int i, command_pos = 0, command_args = 0;
	char **rest = NULL;
	char *fullpath, *launch_params;

	sropt.pack_grp_bits = bit_alloc(MAX_PACK_COUNT);
	bit_set(sropt.pack_grp_bits, pack_offset);

	if ((opt.pn_min_memory > -1) && (opt.mem_per_cpu > -1)) {
		if (opt.pn_min_memory < opt.mem_per_cpu) {
			info("mem < mem-per-cpu - resizing mem to be equal "
			     "to mem-per-cpu");
			opt.pn_min_memory = opt.mem_per_cpu;
		}
	}

	if (sropt.pty) {
		char *launch_type = slurm_get_launch_type();
		if (xstrcmp(launch_type, "launch/slurm")) {
			error("--pty not currently supported with %s "
			      "configuration, ignoring option", launch_type);
			sropt.pty = false;
		}
		xfree(launch_type);
	}

#ifdef HAVE_NATIVE_CRAY
	/* only fatal on the allocation */
	if (opt.network && opt.shared && (opt.jobid == NO_VAL))
		fatal("Requesting network performance counters requires "
		      "exclusive access.  Please add the --exclusive option "
		      "to your request.");
	if (opt.network)
		setenv("SLURM_NETWORK", opt.network, 1);
#endif

	if (opt.dependency)
		setenvfs("SLURM_JOB_DEPENDENCY=%s", opt.dependency);

	sropt.argc = 0;
	if (optind < argc) {
		rest = argv + optind;
		while ((rest[sropt.argc] != NULL) && strcmp(rest[sropt.argc], ":"))
			sropt.argc++;
	}

	command_args = sropt.argc;

	if (!xstrcmp(opt.mpi_type, "list"))
		(void) mpi_hook_client_init(opt.mpi_type);
	if (!rest && !sropt.test_only)
		fatal("No command given to execute.");

	if (launch_init() != SLURM_SUCCESS) {
		fatal("Unable to load launch plugin, check LaunchType "
		      "configuration");
	}
	command_pos = launch_g_setup_srun_opt(rest, &opt);

	/* make sure we have allocated things correctly */
	if (command_args)
		xassert((command_pos + command_args) <= sropt.argc);

	for (i = command_pos; i < sropt.argc; i++) {
		if (!rest || !rest[i-command_pos])
			break;
		sropt.argv[i] = xstrdup(rest[i-command_pos]);
		// info("argv[%d]='%s'", i, sropt.argv[i]);
	}
	sropt.argv[i] = NULL;	/* End of argv's (for possible execv) */

	if (getenv("SLURM_TEST_EXEC")) {
		sropt.test_exec = true;
	} else {
		launch_params = slurm_get_launch_params();
		if (launch_params && strstr(launch_params, "test_exec"))
			sropt.test_exec = true;
		xfree(launch_params);
	}

	if (sropt.test_exec) {
		/* Validate command's existence */
		if (sropt.prolog && xstrcasecmp(sropt.prolog, "none")) {
			if ((fullpath = search_path(opt.cwd, sropt.prolog,
						    true, R_OK|X_OK, true)))
				sropt.prolog = fullpath;
			else
				error("prolog '%s' not found in PATH or CWD (%s), or wrong permissions",
				      sropt.prolog, opt.cwd);
		}
		if (sropt.epilog && xstrcasecmp(sropt.epilog, "none")) {
			if ((fullpath = search_path(opt.cwd, sropt.epilog,
						    true, R_OK|X_OK, true)))
				sropt.epilog = fullpath;
			else
				error("epilog '%s' not found in PATH or CWD (%s), or wrong permissions",
				      sropt.epilog, opt.cwd);
		}
		if (sropt.task_prolog) {
			if ((fullpath = search_path(opt.cwd, sropt.task_prolog,
						    true, R_OK|X_OK, true)))
				sropt.task_prolog = fullpath;
			else
				error("task-prolog '%s' not found in PATH or CWD (%s), or wrong permissions",
				      sropt.task_prolog, opt.cwd);
		}
		if (sropt.task_epilog) {
			if ((fullpath = search_path(opt.cwd, sropt.task_epilog,
						    true, R_OK|X_OK, true)))
				sropt.task_epilog = fullpath;
			else
				error("task-epilog '%s' not found in PATH or CWD (%s), or wrong permissions",
				      sropt.task_epilog, opt.cwd);
		}
	}

	/* may exit() if an error with the multi_prog script */
	(void) launch_g_handle_multi_prog_verify(command_pos, &opt);

	if (!sropt.multi_prog && (sropt.test_exec || sropt.bcast_flag)) {

		if ((fullpath = search_path(opt.cwd, sropt.argv[command_pos],
					    true, X_OK, true))) {
			xfree(sropt.argv[command_pos]);
			sropt.argv[command_pos] = fullpath;
		} else {
			fatal("Can not execute %s", sropt.argv[command_pos]);
		}
	}
}

/*
 * _opt_verify : perform some post option processing verification
 *
 */
static bool _opt_verify(void)
{
	bool verified = true;
	hostlist_t hl = NULL;
	int hl_cnt = 0;

	/*
	 *  Do not set slurmd debug level higher than DEBUG2,
	 *   as DEBUG3 is used for slurmd IO operations, which
	 *   are not appropriate to be sent back to srun. (because
	 *   these debug messages cause the generation of more
	 *   debug messages ad infinitum)
	 */
	if (sropt.slurmd_debug + LOG_LEVEL_ERROR > LOG_LEVEL_DEBUG2) {
		sropt.slurmd_debug = LOG_LEVEL_DEBUG2 - LOG_LEVEL_ERROR;
		info("Using srun's max debug increment of %d",
		     sropt.slurmd_debug);
	}

	if (opt.quiet && opt.verbose) {
		error ("don't specify both --verbose (-v) and --quiet (-Q)");
		verified = false;
	}

	if (sropt.no_alloc && !opt.nodelist) {
		error("must specify a node list with -Z, --no-allocate.");
		verified = false;
	}

	if (sropt.no_alloc && opt.exc_nodes) {
		error("can not specify --exclude list with -Z, --no-allocate.");
		verified = false;
	}

	if (sropt.no_alloc && sropt.relative_set) {
		error("do not specify -r,--relative with -Z,--no-allocate.");
		verified = false;
	}

	if (sropt.relative_set && (opt.exc_nodes || opt.nodelist)) {
		error("-r,--relative not allowed with "
		      "-w,--nodelist or -x,--exclude.");
		verified = false;
	}

	/* DMJ TODO */
	if (opt.hint_env &&
	    (!opt.hint_set &&
	     ((sropt.cpu_bind_type == CPU_BIND_VERBOSE) ||
	      !sropt.cpu_bind_type_set) &&
	     !opt.ntasks_per_core_set && !opt.threads_per_core_set)) {
		if (verify_hint(opt.hint_env,
				&opt.sockets_per_node,
				&opt.cores_per_socket,
				&opt.threads_per_core,
				&opt.ntasks_per_core,
				&sropt.cpu_bind_type)) {
			exit(error_exit);
		}
	}

	if (opt.cpus_set && (opt.pn_min_cpus < opt.cpus_per_task))
		opt.pn_min_cpus = opt.cpus_per_task;

	if ((sropt.argc > 0) && xstrcmp(sropt.argv[0], ":")) {
		xfree(sropt.cmd_name);
		sropt.cmd_name = base_name(sropt.argv[0]);
	}

	if (!opt.nodelist) {
		if ((opt.nodelist = xstrdup(getenv("SLURM_HOSTFILE")))) {
			/* make sure the file being read in has a / in
			   it to make sure it is a file in the
			   valid_node_list function */
			if (!strstr(opt.nodelist, "/")) {
				char *add_slash = xstrdup("./");
				xstrcat(add_slash, opt.nodelist);
				xfree(opt.nodelist);
				opt.nodelist = add_slash;
			}
			opt.distribution &= SLURM_DIST_STATE_FLAGS;
			opt.distribution |= SLURM_DIST_ARBITRARY;
			xfree(sropt.hostfile);
			sropt.hostfile = xstrdup(opt.nodelist);
			if (!_valid_node_list(&opt.nodelist)) {
				error("Failure getting NodeNames from "
				      "hostfile");
				exit(error_exit);
			} else {
				debug("loaded nodes (%s) from hostfile",
				      opt.nodelist);
			}
		}
	} else {
		xfree(sropt.hostfile);
		if (strstr(opt.nodelist, "/"))
			sropt.hostfile = xstrdup(opt.nodelist);
		if (!_valid_node_list(&opt.nodelist))
			exit(error_exit);
	}

	/* set proc and node counts based on the arbitrary list of nodes */
	if (((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_ARBITRARY)
	   && (!opt.nodes_set || !opt.ntasks_set)) {
		hostlist_t hl = hostlist_create(opt.nodelist);
		if (!opt.ntasks_set) {
			opt.ntasks_set = true;
			opt.ntasks = hostlist_count(hl);
		}
		if (!opt.nodes_set) {
			opt.nodes_set = true;
			sropt.nodes_set_opt = true;
			hostlist_uniq(hl);
			opt.min_nodes = opt.max_nodes = hostlist_count(hl);
		}
		hostlist_destroy(hl);
	}

	/*
	 * now if max is set make sure we have <= max_nodes in the
	 * nodelist but only if it isn't arbitrary since the user has
	 * laid it out how it should be so don't mess with it print an
	 * error later if it doesn't work the way they wanted
	 */
	if (opt.max_nodes && opt.nodelist &&
	    ((opt.distribution & SLURM_DIST_STATE_BASE)!=SLURM_DIST_ARBITRARY)) {
		hostlist_t hl = hostlist_create(opt.nodelist);
		int count = hostlist_count(hl);
		if (count > opt.max_nodes) {
			int i = 0;
			error("Required nodelist includes more nodes than "
			      "permitted by max-node count (%d > %d). "
			      "Eliminating nodes from the nodelist.",
			      count, opt.max_nodes);
			count -= opt.max_nodes;
			while (i<count) {
				char *name = hostlist_pop(hl);
				if (name)
					free(name);
				else
					break;
				i++;
			}
			xfree(opt.nodelist);
			opt.nodelist = hostlist_ranged_string_xmalloc(hl);
		}
		hostlist_destroy(hl);
	}

	/* check for realistic arguments */
	if (opt.ntasks <= 0) {
		error("invalid number of tasks (-n %d)", opt.ntasks);
		verified = false;
	}

	if ((opt.min_nodes < 0) || (opt.max_nodes < 0) ||
	    (opt.max_nodes && (opt.min_nodes > opt.max_nodes))) {
		error("invalid number of nodes (-N %d-%d)",
		      opt.min_nodes, opt.max_nodes);
		verified = false;
	}

	if (!opt.ntasks_per_node) {
		error("ntasks-per-node is 0");
		verified = false;
	}


	/* bound max_threads/cores from ntasks_cores/sockets */
	if (opt.ntasks_per_core > 0) {
		/* if cpu_bind_type doesn't already have a auto pref,
		 * choose the level based on the level of ntasks
		 */
		if (!(sropt.cpu_bind_type & (CPU_BIND_TO_SOCKETS |
					   CPU_BIND_TO_CORES |
					   CPU_BIND_TO_THREADS |
					   CPU_BIND_TO_LDOMS |
					   CPU_BIND_TO_BOARDS))) {
			sropt.cpu_bind_type |= CPU_BIND_TO_CORES;
		}
	}
	if (opt.ntasks_per_socket > 0) {
		/* if cpu_bind_type doesn't already have a auto pref,
		 * choose the level based on the level of ntasks
		 */
		if (!(sropt.cpu_bind_type & (CPU_BIND_TO_SOCKETS |
					   CPU_BIND_TO_CORES |
					   CPU_BIND_TO_THREADS |
					   CPU_BIND_TO_LDOMS |
					   CPU_BIND_TO_BOARDS))) {
			sropt.cpu_bind_type |= CPU_BIND_TO_SOCKETS;
		}
	}

	/* massage the numbers */
	if (opt.nodelist) {
		hl = hostlist_create(opt.nodelist);
		if (!hl) {
			error("memory allocation failure");
			exit(error_exit);
		}
		hostlist_uniq(hl);
		hl_cnt = hostlist_count(hl);
		if (opt.nodes_set)
			opt.min_nodes = MAX(hl_cnt, opt.min_nodes);
		else
			opt.min_nodes = hl_cnt;
		opt.nodes_set = true;
	}

	if ((opt.nodes_set || opt.extra_set)				&&
	    ((opt.min_nodes == opt.max_nodes) || (opt.max_nodes == 0))	&&
	    !opt.ntasks_set) {
		/* 1 proc / node default */
		opt.ntasks = opt.min_nodes;

		/* 1 proc / min_[socket * core * thread] default */
		if ((opt.sockets_per_node != NO_VAL) &&
		    (opt.cores_per_socket != NO_VAL) &&
		    (opt.threads_per_core != NO_VAL)) {
			opt.ntasks *= opt.sockets_per_node;
			opt.ntasks *= opt.cores_per_socket;
			opt.ntasks *= opt.threads_per_core;
			opt.ntasks_set = true;
		} else if (opt.ntasks_per_node != NO_VAL) {
			opt.ntasks *= opt.ntasks_per_node;
			opt.ntasks_set = true;
		}

		/* massage the numbers */
		if (opt.nodelist) {
			if (hl)	/* possibly built above */
				hostlist_destroy(hl);
			hl = hostlist_create(opt.nodelist);
			if (!hl) {
				error("memory allocation failure");
				exit(error_exit);
			}
			if (((opt.distribution & SLURM_DIST_STATE_BASE) ==
			     SLURM_DIST_ARBITRARY) && !opt.ntasks_set) {
				opt.ntasks = hostlist_count(hl);
				opt.ntasks_set = true;
			}
			hostlist_uniq(hl);
			hl_cnt = hostlist_count(hl);
			if (opt.nodes_set)
				opt.min_nodes = MAX(hl_cnt, opt.min_nodes);
			else
				opt.min_nodes = hl_cnt;
			/* Don't destroy hl here since it may be used later */
		}
	} else if (opt.nodes_set && opt.ntasks_set) {
		/*
		 * Make sure that the number of
		 * max_nodes is <= number of tasks
		 */
		if (opt.ntasks < opt.max_nodes)
			opt.max_nodes = opt.ntasks;

		/*
		 *  make sure # of procs >= min_nodes
		 */
		if ((opt.ntasks < opt.min_nodes) && (opt.ntasks > 0)) {
			info ("Warning: can't run %d processes on %d "
			      "nodes, setting nnodes to %d",
			      opt.ntasks, opt.min_nodes, opt.ntasks);
			opt.min_nodes = opt.ntasks;
			sropt.nodes_set_opt = true;
			if (opt.max_nodes
			    &&  (opt.min_nodes > opt.max_nodes) )
				opt.max_nodes = opt.min_nodes;
			if (hl_cnt > opt.min_nodes) {
				int del_cnt, i;
				char *host;
				del_cnt = hl_cnt - opt.min_nodes;
				for (i=0; i<del_cnt; i++) {
					host = hostlist_pop(hl);
					free(host);
				}
				xfree(opt.nodelist);
				opt.nodelist =
					hostlist_ranged_string_xmalloc(hl);
			}
		}

		if ((opt.ntasks_per_node != NO_VAL) && opt.min_nodes &&
		    (opt.ntasks_per_node != (opt.ntasks / opt.min_nodes))) {
			if (opt.ntasks > opt.ntasks_per_node)
				info("Warning: can't honor --ntasks-per-node "
				     "set to %u which doesn't match the "
				     "requested tasks %u with the number of "
				     "requested nodes %u. Ignoring "
				     "--ntasks-per-node.", opt.ntasks_per_node,
				     opt.ntasks, opt.min_nodes);
			opt.ntasks_per_node = NO_VAL;
		}

	} /* else if (opt.ntasks_set && !opt.nodes_set) */

	if ((opt.ntasks_per_node != NO_VAL) && (!opt.ntasks_set)) {
		opt.ntasks = opt.min_nodes * opt.ntasks_per_node;
		opt.ntasks_set = 1;
	}

	if (hl)
		hostlist_destroy(hl);

	if (sropt.max_threads <= 0) {	/* set default */
		error("Thread value invalid, reset to 1");
		sropt.max_threads = 1;
		pmi_server_max_threads(sropt.max_threads);
	} else if (sropt.max_threads > MAX_THREADS) {
		error("Thread value exceeds defined limit, reset to %d",
		      MAX_THREADS);
	}

	if ((opt.deadline) && (opt.begin) && (opt.deadline < opt.begin)) {
		error("Incompatible begin and deadline time specification");
		exit(error_exit);
	}

	if (!sropt.ckpt_dir)
		sropt.ckpt_dir = xstrdup(opt.cwd);

	if ((opt.euid != (uid_t) -1) && (opt.euid != opt.uid))
		opt.uid = opt.euid;

	if ((opt.egid != (gid_t) -1) && (opt.egid != opt.gid))
		opt.gid = opt.egid;

	if (!opt.mpi_type)
		opt.mpi_type = slurm_get_mpi_default();
	if (mpi_hook_client_init(opt.mpi_type) == SLURM_ERROR) {
		error("invalid MPI type '%s', --mpi=list for acceptable types",
		      opt.mpi_type);
		exit(error_exit);
	}

	if (!opt.job_name)
		opt.job_name = xstrdup(sropt.cmd_name);

	if (opt.x11) {
		opt.x11_target_port = x11_get_display_port();
		opt.x11_magic_cookie = x11_get_xauth();
	}

	return verified;
}

/* Initialize the spank_job_env based upon environment variables set
 *	via salloc or sbatch commands */
extern void init_spank_env(void)
{
	int i;
	char *name, *eq, *value;

	if (environ == NULL)
		return;

	for (i = 0; environ[i]; i++) {
		if (xstrncmp(environ[i], "SLURM_SPANK_", 12))
			continue;
		name = xstrdup(environ[i] + 12);
		eq = strchr(name, (int)'=');
		if (eq == NULL) {
			xfree(name);
			break;
		}
		eq[0] = '\0';
		value = eq + 1;
		spank_set_job_env(name, value, 1);
		xfree(name);
	}

}

/* Functions used by SPANK plugins to read and write job environment
 * variables for use within job's Prolog and/or Epilog */
extern char *spank_get_job_env(const char *name)
{
	int i, len;
	char *tmp_str = NULL;

	if ((name == NULL) || (name[0] == '\0') ||
	    (strchr(name, (int)'=') != NULL)) {
		slurm_seterrno(EINVAL);
		return NULL;
	}

	xstrcat(tmp_str, name);
	xstrcat(tmp_str, "=");
	len = strlen(tmp_str);

	for (i = 0; i < opt.spank_job_env_size; i++) {
		if (xstrncmp(opt.spank_job_env[i], tmp_str, len))
			continue;
		xfree(tmp_str);
		return (opt.spank_job_env[i] + len);
	}

	return NULL;
}

extern int   spank_set_job_env(const char *name, const char *value,
			       int overwrite)
{
	int i, len;
	char *tmp_str = NULL;

	if ((name == NULL) || (name[0] == '\0') ||
	    (strchr(name, (int)'=') != NULL)) {
		slurm_seterrno(EINVAL);
		return -1;
	}

	xstrcat(tmp_str, name);
	xstrcat(tmp_str, "=");
	len = strlen(tmp_str);
	xstrcat(tmp_str, value);

	for (i = 0; i < opt.spank_job_env_size; i++) {
		if (xstrncmp(opt.spank_job_env[i], tmp_str, len))
			continue;
		if (overwrite) {
			xfree(opt.spank_job_env[i]);
			opt.spank_job_env[i] = tmp_str;
		} else
			xfree(tmp_str);
		return 0;
	}

	/* Need to add an entry */
	opt.spank_job_env_size++;
	xrealloc(opt.spank_job_env, sizeof(char *) * opt.spank_job_env_size);
	opt.spank_job_env[i] = tmp_str;
	return 0;
}

extern int   spank_unset_job_env(const char *name)
{
	int i, j, len;
	char *tmp_str = NULL;

	if ((name == NULL) || (name[0] == '\0') ||
	    (strchr(name, (int)'=') != NULL)) {
		slurm_seterrno(EINVAL);
		return -1;
	}

	xstrcat(tmp_str, name);
	xstrcat(tmp_str, "=");
	len = strlen(tmp_str);

	for (i = 0; i < opt.spank_job_env_size; i++) {
		if (xstrncmp(opt.spank_job_env[i], tmp_str, len))
			continue;
		xfree(opt.spank_job_env[i]);
		for (j = (i+1); j < opt.spank_job_env_size; i++, j++)
			opt.spank_job_env[i] = opt.spank_job_env[j];
		opt.spank_job_env_size--;
		if (opt.spank_job_env_size == 0)
			xfree(opt.spank_job_env);
		return 0;
	}

	return 0;	/* not found */
}

/* helper function for printing options
 *
 * warning: returns pointer to memory allocated on the stack.
 */
static char *print_constraints(void)
{
	char *buf = xstrdup("");

	if (opt.pn_min_cpus != NO_VAL)
		xstrfmtcat(buf, "mincpus-per-node=%d ", opt.pn_min_cpus);

	if (opt.pn_min_memory != NO_VAL64)
		xstrfmtcat(buf, "mem-per-node=%"PRIi64"M ", opt.pn_min_memory);

	if (opt.mem_per_cpu != NO_VAL64)
		xstrfmtcat(buf, "mem-per-cpu=%"PRIi64"M ", opt.mem_per_cpu);

	if (opt.pn_min_tmp_disk != NO_VAL)
		xstrfmtcat(buf, "tmp-per-node=%ld ", opt.pn_min_tmp_disk);

	if (opt.contiguous == true)
		xstrcat(buf, "contiguous ");

	if (opt.nodelist != NULL)
		xstrfmtcat(buf, "nodelist=%s ", opt.nodelist);

	if (opt.exc_nodes != NULL)
		xstrfmtcat(buf, "exclude=%s ", opt.exc_nodes);

	if (opt.constraints != NULL)
		xstrfmtcat(buf, "constraints=`%s' ", opt.constraints);

	if (opt.c_constraints != NULL)
		xstrfmtcat(buf, "clsuter-constraints=`%s' ", opt.c_constraints);

	return buf;
}

#define tf_(b) (b == true) ? "true" : "false"

static void _opt_list(void)
{
	char *str;
	int i;

	info("defined options for program `%s'", opt.progname);
	info("--------------- ---------------------");

	info("user           : `%s'", opt.user);
	info("uid            : %ld", (long) opt.uid);
	info("gid            : %ld", (long) opt.gid);
	info("cwd            : %s", opt.cwd);
	info("ntasks         : %d %s", opt.ntasks,
	     opt.ntasks_set ? "(set)" : "(default)");
	if (opt.cpus_set)
		info("cpus_per_task  : %d", opt.cpus_per_task);
	if (opt.max_nodes)
		info("nodes          : %d-%d", opt.min_nodes, opt.max_nodes);
	else {
		info("nodes          : %d %s", opt.min_nodes,
		     opt.nodes_set ? "(set)" : "(default)");
	}
	info("jobid          : %u %s", opt.jobid,
	     opt.jobid_set ? "(set)" : "(default)");
	info("partition      : %s",
	     opt.partition == NULL ? "default" : opt.partition);
	info("profile        : `%s'",
	     acct_gather_profile_to_string(opt.profile));
	info("job name       : `%s'", opt.job_name);
	info("reservation    : `%s'", opt.reservation);
	info("burst_buffer   : `%s'", opt.burst_buffer);
	info("wckey          : `%s'", opt.wckey);
	info("cpu_freq_min   : %u", opt.cpu_freq_min);
	info("cpu_freq_max   : %u", opt.cpu_freq_max);
	info("cpu_freq_gov   : %u", opt.cpu_freq_gov);
	if (opt.delay_boot != NO_VAL)
		info("delay_boot        : %u", opt.delay_boot);
	info("switches       : %d", opt.req_switch);
	info("wait-for-switches : %d", opt.wait4switch);
	info("distribution   : %s", format_task_dist_states(opt.distribution));
	if ((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_PLANE)
		info("plane size   : %u", opt.plane_size);
	info("cpu-bind       : %s (%u)",
	     sropt.cpu_bind == NULL ? "default" : sropt.cpu_bind, sropt.cpu_bind_type);
	info("mem-bind       : %s (%u)",
	     opt.mem_bind == NULL ? "default" : opt.mem_bind, opt.mem_bind_type);
	info("verbose        : %d", opt.verbose);
	info("slurmd_debug   : %d", sropt.slurmd_debug);
	if (opt.immediate <= 1)
		info("immediate      : %s", tf_(opt.immediate));
	else
		info("immediate      : %d secs", (opt.immediate - 1));
	info("label output   : %s", tf_(sropt.labelio));
	info("unbuffered IO  : %s", tf_(sropt.unbuffered));
	info("overcommit     : %s", tf_(opt.overcommit));
	info("threads        : %d", sropt.max_threads);
	if (opt.time_limit == INFINITE)
		info("time_limit     : INFINITE");
	else if (opt.time_limit != NO_VAL)
		info("time_limit     : %d", opt.time_limit);
	if (opt.time_min != NO_VAL)
		info("time_min       : %d", opt.time_min);
	if (sropt.ckpt_interval)
		info("checkpoint     : %d mins", sropt.ckpt_interval);
	info("checkpoint_dir : %s", sropt.ckpt_dir);
	if (sropt.restart_dir)
		info("restart_dir    : %s", sropt.restart_dir);
	info("wait           : %d", sropt.max_wait);
	if (opt.nice)
		info("nice           : %d", opt.nice);
	info("account        : %s", opt.account);
	info("comment        : %s", opt.comment);

	info("dependency     : %s", opt.dependency);
	if (opt.gres)
		info("gres           : %s", opt.gres);
	info("exclusive      : %s", tf_(sropt.exclusive));
	if (sropt.bcast_file)
		info("bcast          : %s", sropt.bcast_file);
	else
		info("bcast          : %s", tf_(sropt.bcast_flag));
	info("qos            : %s", opt.qos);
	if (opt.shared != NO_VAL16)
		info("oversubscribe  : %u", opt.shared);
	str = print_constraints();
	info("constraints    : %s", str);
	xfree(str);
	info("reboot         : %s", opt.reboot ? "no" : "yes");
	info("preserve_env   : %s", tf_(sropt.preserve_env));

	info("network        : %s", opt.network);
	info("propagate      : %s",
	     sropt.propagate == NULL ? "NONE" : sropt.propagate);
	if (opt.begin) {
		char time_str[32];
		slurm_make_time_str(&opt.begin, time_str, sizeof(time_str));
		info("begin          : %s", time_str);
	}
	if (opt.deadline) {
		char time_str[32];
		slurm_make_time_str(&opt.deadline, time_str, sizeof(time_str));
		info("deadline       : %s", time_str);
	}
	info("prolog         : %s", sropt.prolog);
	info("epilog         : %s", sropt.epilog);
	info("mail_type      : %s", print_mail_type(opt.mail_type));
	info("mail_user      : %s", opt.mail_user);
	info("task_prolog    : %s", sropt.task_prolog);
	info("task_epilog    : %s", sropt.task_epilog);
	info("multi_prog     : %s", sropt.multi_prog ? "yes" : "no");
	info("sockets-per-node  : %d", opt.sockets_per_node);
	info("cores-per-socket  : %d", opt.cores_per_socket);
	info("threads-per-core  : %d", opt.threads_per_core);
	info("ntasks-per-node   : %d", opt.ntasks_per_node);
	info("ntasks-per-socket : %d", opt.ntasks_per_socket);
	info("ntasks-per-core   : %d", opt.ntasks_per_core);
	info("plane_size        : %u", opt.plane_size);
	if (opt.core_spec == NO_VAL16)
		info("core-spec         : NA");
	else if (opt.core_spec & CORE_SPEC_THREAD) {
		info("thread-spec       : %d",
		     opt.core_spec & (~CORE_SPEC_THREAD));
	} else
		info("core-spec         : %d", opt.core_spec);
	if (sropt.resv_port_cnt != NO_VAL)
		info("resv_port_cnt     : %d", sropt.resv_port_cnt);
	info("power             : %s", power_flags_str(opt.power_flags));

	info("cpus-per-gpu      : %d", opt.cpus_per_gpu);
	info("gpus              : %s", opt.gpus);
	info("gpu-bind          : %s", opt.gpu_bind);
	info("gpu-freq          : %s", opt.gpu_freq);
	info("gpus-per-node     : %s", opt.gpus_per_node);
	info("gpus-per-socket   : %s", opt.gpus_per_socket);
	info("gpus-per-task     : %s", opt.gpus_per_task);
	info("mem-per-gpu       : %"PRIi64, opt.mem_per_gpu);
	info("tres-per-job      : %s", opt.tres_per_job);

	str = print_commandline(sropt.argc, sropt.argv);
	info("remote command    : `%s'", str);
	xfree(str);

	if (sropt.pack_group)
		info("pack_group        : %s", sropt.pack_group);

	for (i = 0; i < opt.spank_job_env_size; i++)
		info("spank_job_env[%d] : %s", i, opt.spank_job_env[i]);

}

/* Read specified file's contents into a buffer.
 * Caller must xfree the buffer's contents */
static char *_read_file(char *fname)
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

/* Determine if srun is under the control of a parallel debugger or not */
static bool _under_parallel_debugger (void)
{
	return (MPIR_being_debugged != 0);
}

static void _usage(void)
{
 	printf(
"Usage: srun [-N nnodes] [-n ntasks] [-i in] [-o out] [-e err]\n"
"            [-c ncpus] [-r n] [-p partition] [--hold] [-t minutes]\n"
"            [-D path] [--immediate[=secs]] [--overcommit] [--no-kill]\n"
"            [--oversubscribe] [--label] [--unbuffered] [-m dist] [-J jobname]\n"
"            [--jobid=id] [--verbose] [--slurmd_debug=#] [--gres=list]\n"
"            [-T threads] [-W sec] [--checkpoint=time] [--gres-flags=opts]\n"
"            [--checkpoint-dir=dir] [--licenses=names] [--clusters=cluster_names]\n"
"            [--restart-dir=dir] [--qos=qos] [--time-min=minutes]\n"
"            [--contiguous] [--mincpus=n] [--mem=MB] [--tmp=MB] [-C list]\n"
"            [--mpi=type] [--account=name] [--dependency=type:jobid]\n"
"            [--launch-cmd] [--launcher-opts=options]\n"
"            [--kill-on-bad-exit] [--propagate[=rlimits] [--comment=name]\n"
"            [--cpu-bind=...] [--mem-bind=...] [--network=type]\n"
"            [--ntasks-per-node=n] [--ntasks-per-socket=n] [reservation=name]\n"
"            [--ntasks-per-core=n] [--mem-per-cpu=MB] [--preserve-env]\n"
"            [--profile=...]\n"
"            [--mail-type=type] [--mail-user=user] [--nice[=value]]\n"
"            [--prolog=fname] [--epilog=fname]\n"
"            [--task-prolog=fname] [--task-epilog=fname]\n"
"            [--ctrl-comm-ifhn=addr] [--multi-prog] [--mcs-label=mcs]\n"
"            [--cpu-freq=min[-max[:gov]] [--power=flags] [--spread-job]\n"
"            [--switches=max-switches{@max-time-to-wait}] [--reboot]\n"
"            [--core-spec=cores] [--thread-spec=threads]\n"
"            [--bb=burst_buffer_spec] [--bbf=burst_buffer_file]\n"
"            [--bcast=<dest_path>] [--compress[=library]]\n"
"            [--acctg-freq=<datatype>=<interval>] [--delay-boot=mins]\n"
"            [-w hosts...] [-x hosts...] [--use-min-nodes]\n"
"            [--mpi-combine=yes|no] [--pack-group=value]\n"
"            [--cpus-per-gpu=n] [--gpus=n] [--gpu-bind=...] [--gpu-freq=...]\n"
"            [--gpus-per-node=n] [--gpus-per-socket=n]  [--gpus-per-task=n]\n"
"            [--mem-per-gpu=MB]\n"
"            executable [args...]\n");

}

static void _help(void)
{
	slurm_ctl_conf_t *conf;

        printf (
"Usage: srun [OPTIONS...] executable [args...]\n"
"\n"
"Parallel run options:\n"
"  -A, --account=name          charge job to specified account\n"
"      --acctg-freq=<datatype>=<interval> accounting and profiling sampling\n"
"                              intervals. Supported datatypes:\n"
"                              task=<interval> energy=<interval>\n"
"                              network=<interval> filesystem=<interval>\n"
"      --bb=<spec>             burst buffer specifications\n"
"      --bbf=<file_name>       burst buffer specification file\n"
"      --bcast=<dest_path>     Copy executable file to compute nodes\n"
"      --begin=time            defer job until HH:MM MM/DD/YY\n"
"  -c, --cpus-per-task=ncpus   number of cpus required per task\n"
"      --checkpoint=time       job step checkpoint interval\n"
"      --checkpoint-dir=dir    directory to store job step checkpoint image \n"
"                              files\n"
"      --comment=name          arbitrary comment\n"
"      --compress[=library]    data compression library used with --bcast\n"
"      --cpu-freq=min[-max[:gov]] requested cpu frequency (and governor)\n"
"  -d, --dependency=type:jobid defer job until condition on jobid is satisfied\n"
"      --deadline=time         remove the job if no ending possible before\n"
"                              this deadline (start > (deadline - time[-min]))\n"
"      --delay-boot=mins       delay boot for desired node features\n"
"  -D, --chdir=path            change remote current working directory\n"
"      --export=env_vars|NONE  environment variables passed to launcher with\n"
"                              optional values or NONE (pass no variables)\n"
"  -e, --error=err             location of stderr redirection\n"
"      --epilog=program        run \"program\" after launching job step\n"
"  -E, --preserve-env          env vars for node and task counts override\n"
"                              command-line flags\n"
"      --get-user-env          used by Moab.  See srun man page.\n"
"      --gres=list             required generic resources\n"
"      --gres-flags=opts       flags related to GRES management\n"
"  -H, --hold                  submit job in held state\n"
"  -i, --input=in              location of stdin redirection\n"
"  -I, --immediate[=secs]      exit if resources not available in \"secs\"\n"
"      --jobid=id              run under already allocated job\n"
"  -J, --job-name=jobname      name of job\n"
"  -k, --no-kill               do not kill job on node failure\n"
"  -K, --kill-on-bad-exit      kill the job if any task terminates with a\n"
"                              non-zero exit code\n"
"  -l, --label                 prepend task number to lines of stdout/err\n"
"      --launch-cmd            print external launcher command line if not Slurm\n"
"      --launcher-opts=        options for the external launcher command if not\n"
"                              Slurm\n"
"  -L, --licenses=names        required license, comma separated\n"
"  -M, --clusters=names        Comma separated list of clusters to issue\n"
"                              commands to.  Default is current cluster.\n"
"                              Name of 'all' will submit to run on all clusters.\n"
"                              NOTE: SlurmDBD must up.\n"
"  -m, --distribution=type     distribution method for processes to nodes\n"
"                              (type = block|cyclic|arbitrary)\n"
"      --mail-type=type        notify on state change: BEGIN, END, FAIL or ALL\n"
"      --mail-user=user        who to send email notification for job state\n"
"                              changes\n"
"      --mcs-label=mcs         mcs label if mcs plugin mcs/group is used\n"
"      --mpi=type              type of MPI being used\n"
"      --multi-prog            if set the program name specified is the\n"
"                              configuration specification for multiple programs\n"
"  -n, --ntasks=ntasks         number of tasks to run\n"
"      --nice[=value]          decrease scheduling priority by value\n"
"      --ntasks-per-node=n     number of tasks to invoke on each node\n"
"  -N, --nodes=N               number of nodes on which to run (N = min[-max])\n"
"  -o, --output=out            location of stdout redirection\n"
"  -O, --overcommit            overcommit resources\n"
"      --pack-group=value      pack job allocation(s) in which to launch\n"
"                              application\n"
"  -p, --partition=partition   partition requested\n"
"      --power=flags           power management options\n"
"      --priority=value        set the priority of the job to value\n"
"      --prolog=program        run \"program\" before launching job step\n"
"      --profile=value         enable acct_gather_profile for detailed data\n"
"                              value is all or none or any combination of\n"
"                              energy, lustre, network or task\n"
"      --propagate[=rlimits]   propagate all [or specific list of] rlimits\n"
#ifdef HAVE_PTY_H
"      --pty                   run task zero in pseudo terminal\n"
#endif
"      --quit-on-interrupt     quit on single Ctrl-C\n"
"  -q, --qos=qos               quality of service\n"
"  -Q, --quiet                 quiet mode (suppress informational messages)\n"
"      --reboot                reboot block before starting job\n"
"  -r, --relative=n            run job step relative to node n of allocation\n"
"      --restart-dir=dir       directory of checkpoint image files to restart\n"
"                              from\n"
"  -s, --oversubscribe         over-subscribe resources with other jobs\n"
"  -S, --core-spec=cores       count of reserved cores\n"
"      --signal=[B:]num[@time] send signal when time limit within time seconds\n"
"      --slurmd-debug=level    slurmd debug level\n"
"      --spread-job            spread job across as many nodes as possible\n"
"      --switches=max-switches{@max-time-to-wait}\n"
"                              Optimum switches and max time to wait for optimum\n"
"      --task-epilog=program   run \"program\" after launching task\n"
"      --task-prolog=program   run \"program\" before launching task\n"
"      --thread-spec=threads   count of reserved threads\n"
"  -T, --threads=threads       set srun launch fanout\n"
"  -t, --time=minutes          time limit\n"
"      --time-min=minutes      minimum time limit (if distinct)\n"
"  -u, --unbuffered            do not line-buffer stdout/err\n"
"      --use-min-nodes         if a range of node counts is given, prefer the\n"
"                              smaller count\n"
"  -v, --verbose               verbose mode (multiple -v's increase verbosity)\n"
"  -W, --wait=sec              seconds to wait after first task exits\n"
"                              before killing job\n"
"      --wckey=wckey           wckey to run job under\n"
"  -X, --disable-status        Disable Ctrl-C status feature\n"
"\n"
"Constraint options:\n"
"      --cluster-constraint=list specify a list of cluster-constraints\n"
"      --contiguous            demand a contiguous range of nodes\n"
"  -C, --constraint=list       specify a list of constraints\n"
"      --mem=MB                minimum amount of real memory\n"
"      --mincpus=n             minimum number of logical processors (threads)\n"
"                              per node\n"
"      --reservation=name      allocate resources from named reservation\n"
"      --tmp=MB                minimum amount of temporary disk\n"
"  -w, --nodelist=hosts...     request a specific list of hosts\n"
"  -x, --exclude=hosts...      exclude a specific list of hosts\n"
"  -Z, --no-allocate           don't allocate nodes (must supply -w)\n"
"\n"
"Consumable resources related options:\n"
"      --exclusive[=user]      allocate nodes in exclusive mode when\n"
"                              cpu consumable resource is enabled\n"
"                              or don't share CPUs for job steps\n"
"      --exclusive[=mcs]       allocate nodes in exclusive mode when\n"
"                              cpu consumable resource is enabled\n"
"                              and mcs plugin is enabled\n"
"                              or don't share CPUs for job steps\n"
"      --mem-per-cpu=MB        maximum amount of real memory per allocated\n"
"                              cpu required by the job.\n"
"                              --mem >= --mem-per-cpu if --mem is specified.\n"
"      --resv-ports            reserve communication ports\n"
"\n"
"Affinity/Multi-core options: (when the task/affinity plugin is enabled)\n"
"  -B, --extra-node-info=S[:C[:T]]           Expands to:\n"
"      --sockets-per-node=S    number of sockets per node to allocate\n"
"      --cores-per-socket=C    number of cores per socket to allocate\n"
"      --threads-per-core=T    number of threads per core to allocate\n"
"                              each field can be 'min' or wildcard '*'\n"
"                              total cpus requested = (N x S x C x T)\n"
"\n"
"      --ntasks-per-core=n     number of tasks to invoke on each core\n"
"      --ntasks-per-socket=n   number of tasks to invoke on each socket\n");
	conf = slurm_conf_lock();
	if (xstrstr(conf->task_plugin, "affinity") ||
	    xstrstr(conf->task_plugin, "cgroup")) {
		printf(
"      --cpu-bind=             Bind tasks to CPUs\n"
"                              (see \"--cpu-bind=help\" for options)\n"
"      --hint=                 Bind tasks according to application hints\n"
"                              (see \"--hint=help\" for options)\n");
	}
	if (xstrstr(conf->task_plugin, "affinity")) {
		printf(
"      --mem-bind=             Bind memory to locality domains (ldom)\n"
"                              (see \"--mem-bind=help\" for options)\n");
	}
	slurm_conf_unlock();

	spank_print_options(stdout, 6, 30);

	printf("\n"
"GPU scheduling options:\n"
"      --cpus-per-gpu=n        number of CPUs required per allocated GPU\n"
"  -G, --gpus=n                count of GPUs required for the job\n"
"      --gpu-bind=...          task to gpu binding options\n"
"      --gpu-freq=...          frequency and voltage of GPUs\n"
"      --gpus-per-node=n       number of GPUs required per allocated node\n"
"      --gpus-per-socket=n     number of GPUs required per allocated socket\n"
"      --gpus-per-task=n       number of GPUs required per spawned task\n"
"      --mem-per-gpu=n         real memory required per allocated GPU\n"
		);

	printf("\n"
#ifdef HAVE_NATIVE_CRAY			/* Native Cray specific options */
"Cray related options:\n"
"      --network=type          Use network performance counters\n"
"                              (system, network, or processor)\n"
"\n"
#endif
"Help options:\n"
"  -h, --help                  show this help message\n"
"      --usage                 display brief usage message\n"
"\n"
"Other options:\n"
"  -V, --version               output version information and exit\n"
"\n"
		);

}
