/*****************************************************************************\
 *  opt.c - options processing for salloc
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

#include <ctype.h>		/* isdigit    */
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>		/* getpwuid   */
#include <stdarg.h>		/* va_start   */
#include <stdio.h>
#include <stdlib.h>		/* getenv, strtol, etc. */
#include <sys/param.h>		/* MAXPATHLEN */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "src/common/cpu_frequency.h"
#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/parse_time.h"
#include "src/common/plugstack.h"
#include "src/common/proc_args.h"
#include "src/common/read_config.h" /* contains getnodename() */
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_resource_info.h"
#include "src/common/slurm_rlimits_info.h"
#include "src/common/slurm_acct_gather_profile.h"
#include "src/common/uid.h"
#include "src/common/x11_util.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/util-net.h"
#include "src/salloc/salloc.h"
#include "src/salloc/opt.h"

/* generic OPT_ definitions -- mainly for use with env vars  */
#define OPT_NONE        0x00
#define OPT_INT         0x01
#define OPT_STRING      0x02
#define OPT_DEBUG       0x03
#define OPT_NODES       0x04
#define OPT_BOOL        0x05
#define OPT_CORE        0x06
#define OPT_BELL        0x0a
#define OPT_NO_BELL     0x0b
#define OPT_JOBID       0x0c
#define OPT_EXCLUSIVE   0x0d
#define OPT_OVERCOMMIT  0x0e
#define OPT_ACCTG_FREQ  0x0f
#define OPT_GRES_FLAGS  0x10
#define OPT_MEM_BIND    0x11
#define OPT_IMMEDIATE   0x12
#define OPT_POWER       0x13
#define OPT_WCKEY       0x14
#define OPT_SIGNAL      0x15
#define OPT_KILL_CMD    0x16
#define OPT_TIME_VAL	0x17
#define OPT_PROFILE     0x18
#define OPT_CORE_SPEC   0x19
#define OPT_HINT	0x1a
#define OPT_CPU_FREQ    0x1b
#define OPT_THREAD_SPEC 0x1c
#define OPT_SPREAD_JOB  0x1d
#define OPT_DELAY_BOOT	0x1e
#define OPT_INT64	0x1f
#define OPT_MEM_PER_GPU   0x20
#define OPT_NO_KILL       0x21
#define OPT_USE_MIN_NODES 0x23

/*---- global variables, defined in opt.h ----*/
slurm_opt_t opt;
salloc_opt_t saopt;
int error_exit = 1;
bool first_pass = true;
int immediate_exit = 1;

/*---- forward declarations of static functions  ----*/

typedef struct env_vars env_vars_t;

static void  _help(void);
static void  _opt_default(void);
static void  _opt_env(int pass);
static void  _opt_args(int argc, char **argv);
static void  _opt_list(void);
static bool  _opt_verify(void);
static char *_read_file(char *fname);
static void  _set_options(int argc, char **argv);
static void  _usage(void);

/*---[ end forward declarations of static functions ]---------------------*/

/* process options:
 * 1. set defaults
 * 2. update options with env vars
 * 3. update options with commandline args
 * 4. perform some verification that options are reasonable
 *
 * argc IN - Count of elements in argv
 * argv IN - Array of elements to parse
 * argc_off OUT - Offset of first non-parsable element  */
extern int initialize_and_process_args(int argc, char **argv, int *argc_off)
{
	/* initialize option defaults */
	_opt_default();

	/* initialize options with env vars */
	_opt_env(0);

	/* initialize options with argv */
	_opt_args(argc, argv);
	if (argc_off)
		*argc_off = optind;

	_opt_env(1);

	if (!_opt_verify())
		exit(error_exit);

	if (opt.verbose)
		_opt_list();
	first_pass = false;

	return 1;

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
		opt.progname ? opt.progname : "salloc", buf);
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
	/*
	 * Some options will persist for all components of a heterogeneous job
	 * once specified for one, but will be overwritten with new values if
	 * specified on the command line
	 */
	if (first_pass) {
		opt.salloc_opt = &saopt;
		opt.sbatch_opt = NULL;
		opt.srun_opt = NULL;
		xfree(opt.account);
		xfree(opt.acctg_freq);
		opt.begin		= 0;
		saopt.bell		= BELL_AFTER_DELAY;
		xfree(opt.c_constraints);
		xfree(opt.clusters);
		xfree(opt.comment);
		opt.cpus_per_gpu	= 0;
		xfree(opt.cwd);
		opt.deadline		= 0;
		opt.delay_boot		= NO_VAL;
		xfree(opt.dependency);
		opt.egid		= (gid_t) -1;
		opt.euid		= (uid_t) -1;
		xfree(opt.extra);
		xfree(opt.exc_nodes);
		opt.get_user_env_mode	= -1;
		opt.get_user_env_time	= -1;
		opt.gid			= getgid();
		xfree(opt.gpus);
		xfree(opt.gpu_bind);
		xfree(opt.gpu_freq);
		xfree(opt.gpus_per_node);
		xfree(opt.gpus_per_socket);
		xfree(opt.gpus_per_task);
		opt.hold		= false;
		opt.immediate		= 0;
		xfree(opt.job_name);
		saopt.kill_command_signal = SIGTERM;
		saopt.kill_command_signal_set = false;
		xfree(opt.mcs_label);
		opt.mem_per_gpu		= 0;
		opt.nice		= NO_VAL;
		opt.no_kill		= false;
		saopt.no_shell		= false;
		opt.power_flags		= 0;
		opt.priority		= 0;
		opt.profile		= ACCT_GATHER_PROFILE_NOT_SET;
		xfree(opt.progname);
		xfree(opt.qos);
		opt.quiet		= 0;
		opt.reboot		= false;
		opt.time_limit		= NO_VAL;
		opt.time_min		= NO_VAL;
		xfree(opt.time_min_str);
		opt.uid			= getuid();
		opt.user		= uid_to_string(opt.uid);
		if (xstrcmp(opt.user, "nobody") == 0)
			fatal("Invalid user id: %u", (uint32_t) opt.uid);
		opt.verbose		= 0;
		saopt.wait_all_nodes	= NO_VAL16;
		opt.warn_flags		= 0;
		opt.warn_signal		= 0;
		opt.warn_time		= 0;
		xfree(opt.wckey);
		opt.x11			= 0;
	} else if (saopt.default_job_name) {
		xfree(opt.job_name);
	}

	/* All other options must be specified individually for each component
	 * of the job */
	xfree(opt.burst_buffer);
	xfree(opt.constraints);
	opt.contiguous			= false;
	opt.core_spec			= NO_VAL16;
	opt.cores_per_socket		= NO_VAL; /* requested cores */
	opt.cpu_freq_max		= NO_VAL;
	opt.cpu_freq_gov		= NO_VAL;
	opt.cpu_freq_min		= NO_VAL;
	opt.cpus_per_task		= 0;
	opt.cpus_set			= false;
	saopt.default_job_name		= false;
	opt.distribution		= SLURM_DIST_UNKNOWN;
	xfree(opt.hint_env);
	opt.hint_set			= false;
	xfree(opt.gres);
	opt.job_flags			= 0;
	opt.jobid			= NO_VAL;
	opt.max_nodes			= 0;
	xfree(opt.mem_bind);
	opt.mem_bind_type		= 0;
	opt.mem_per_cpu			= -1;
	opt.pn_min_cpus			= -1;
	opt.min_nodes			= 1;
	opt.ntasks			= 1;
	opt.ntasks_per_node		= 0;  /* ntask max limits */
	opt.ntasks_per_socket		= NO_VAL;
	opt.ntasks_per_core		= NO_VAL;
	opt.ntasks_per_core_set		= false;
	opt.nodes_set			= false;
	xfree(opt.nodelist);
	opt.ntasks_set			= false;
	opt.overcommit			= false;
	xfree(opt.partition);
	opt.plane_size			= NO_VAL;
	opt.pn_min_memory		= -1;
	xfree(opt.reservation);
	opt.req_switch			= -1;
	opt.shared			= NO_VAL16;
	opt.sockets_per_node		= NO_VAL; /* requested sockets */
	opt.threads_per_core		= NO_VAL; /* requested threads */
	opt.threads_per_core_set	= false;
	opt.pn_min_tmp_disk		= -1;
	opt.wait4switch			= -1;

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
        int *(set_func)(slurm_opt_t *, const char *);
        int eval_pass;
        int exit_on_error;
};

env_vars_t env_vars[] = {
  {"SALLOC_ACCOUNT",       &arg_set_account,		0,	0 },
  {"SALLOC_ACCTG_FREQ",    &arg_set_acct_greq,		0,	0 },
  {"SALLOC_BELL",          &arg_set_bell,		0,	0 },
  {"SALLOC_BURST_BUFFER",  &arg_set_bb,			0,	0 },
  {"SALLOC_CLUSTERS",      &arg_set_clusters,		0,	0 },
  {"SLURM_CLUSTERS",       &arg_set_clusters,		0,	0 },
  {"SALLOC_CONSTRAINT",    &arg_set_constraint,		0,	0 },
  {"SALLOC_CLUSTER_CONSTRAINT", &arg_set_cluster_constraint, 0, 0 },
  {"SALLOC_CORE_SPEC",     &arg_set_core_spec,		0,	0 },
  {"SALLOC_CPU_FREQ_REQ",  &arg_set_cpu_freq,		0,	0 },
  {"SALLOC_CPUS_PER_GPU",  &arg_set_cpus_per_gpu,	0,	0 },
  {"SALLOC_DEBUG",         &arg_set_debug,		0,	0 },
  {"SALLOC_DELAY_BOOT",    &arg_set_delay_boot,		0,	0 },
  {"SALLOC_EXCLUSIVE",     &arg_set_exclusive,		0,	0 },
  {"SALLOC_GPUS",          &arg_set_gpus,		0,	0 },
  {"SALLOC_GPU_BIND",      &arg_set_gpu_bind,		0,	0 },
  {"SALLOC_GPU_FREQ",      &arg_set_gpu_freq,		0,	0 },
  {"SALLOC_GPUS_PER_NODE", &arg_set_gpus_per_node,	0,	0 },
  {"SALLOC_GPUS_PER_SOCKET", &arg_set_gpus_per_socket,	0,	0 },
  {"SALLOC_GPUS_PER_TASK", &arg_set_gpus_per_task,	0,	0 },
  {"SALLOC_GRES_FLAGS",    &arg_set_gres_flags,		0,	1 },
  {"SALLOC_IMMEDIATE",     &arg_set_immediate,		0,	0 },
  {"SALLOC_HINT",          &arg_set_hint,		1,	1 },
  {"SLURM_HINT",           &arg_set_hint,		1,	1 },
  {"SALLOC_JOBID",         &arg_set_jobid,		0,	0 },
  {"SALLOC_KILL_CMD",      &arg_set_kill_cmd,		0,	0 },
  {"SALLOC_MEM_BIND",      &arg_set_mem_bind,		0,	1 },
  {"SALLOC_MEM_PER_GPU",   &arg_set_mem_per_gpu,	0,	0 },
  {"SALLOC_NETWORK",       &arg_set_network,		0,	0 },
  {"SALLOC_NO_BELL",       &arg_set_no_bell,		0,	0 },
  {"SALLOC_NO_KILL",	   &arg_set_no_kill,		0,	0 },
  {"SALLOC_OVERCOMMIT",    &arg_set_overcommit,		0,	0 },
  {"SALLOC_PARTITION",     &arg_set_partition,		0,	0 },
  {"SALLOC_POWER",         &arg_set_power,		0,	0 },
  {"SALLOC_PROFILE",       &arg_set_profile,		0,	0 },
  {"SALLOC_QOS",           &arg_set_qos,		0,	0 },
  {"SALLOC_REQ_SWITCH",    &arg_setcomp_req_switch,	0,	0 },
  {"SALLOC_RESERVATION",   &arg_set_reservation,	0,	0 },
  {"SALLOC_SIGNAL",        &arg_set_signal,		0,	1 },
  {"SALLOC_SPREAD_JOB",    &arg_set_spread_job,		0,	0 },
  {"SALLOC_THREAD_SPEC",   &arg_set_thread_spec,	0,	0 },
  {"SALLOC_TIMELIMIT",     &arg_set_time,		0,	1 },
  {"SALLOC_USE_MIN_NODES", &arg_set_use_min_nodes,	0,	0 },
  {"SALLOC_WAIT",          &arg_set_wait,		0,	0 },
  {"SALLOC_WAIT_ALL_NODES",&arg_set_wait_all_nodes,	0,	0 },
  {"SALLOC_WAIT4SWITCH",   &arg_setcomp_wait4switch,	0,	0 },
  {"SALLOC_WCKEY",         &arg_set_wckey,		0,	0 },
  {NULL, NULL, 0, 0}
};


/*
 * _opt_env(int): used by initialize_and_process_args to set options via
 *            environment variables. See comments above for how to
 *            extend srun to process different vars
 */
static void _opt_env(int eval_pass)
{
	char       *val = NULL;
	env_vars_t *e   = env_vars;

	while (e->var) {
		if (e->eval_pass == eval_pass &&
		   (val = getenv(e->var)) != NULL) {
			(e->set_func)(&opt, val, e->var, (bool) e->exit_on_error);
		}
		e++;
	}

	/* Process spank env options */
	if (spank_process_env_options())
		exit(error_exit);
}

static void _set_options(int argc, char **argv)
{
	int opt_char, option_index = 0, max_val = 0, i;
	char *tmp;
	long long priority;
	static struct option long_options[] = {
		{"account",       required_argument, 0, 'A'},
		{"extra-node-info", required_argument, 0, 'B'},
		{"cpus-per-task", required_argument, 0, 'c'},
		{"constraint",    required_argument, 0, 'C'},
		{"cluster-constraint",required_argument, 0, LONG_OPT_CLUSTER_CONSTRAINT},
		{"dependency",    required_argument, 0, 'd'},
		{"chdir",         required_argument, 0, 'D'},
		{"nodefile",      required_argument, 0, 'F'},
		{"gpus",          required_argument, 0, 'G'},
		{"help",          no_argument,       0, 'h'},
		{"hold",          no_argument,       0, 'H'},
		{"immediate",     optional_argument, 0, 'I'},
		{"job-name",      required_argument, 0, 'J'},
		{"no-kill",       optional_argument, 0, 'k'},
		{"kill-command",  optional_argument, 0, 'K'},
		{"licenses",      required_argument, 0, 'L'},
		{"distribution",  required_argument, 0, 'm'},
		{"cluster",       required_argument, 0, 'M'},
		{"clusters",      required_argument, 0, 'M'},
		{"tasks",         required_argument, 0, 'n'},
		{"ntasks",        required_argument, 0, 'n'},
		{"nodes",         required_argument, 0, 'N'},
		{"overcommit",    no_argument,       0, 'O'},
		{"oversubscribe", no_argument,       0, 's'},
		{"partition",     required_argument, 0, 'p'},
		{"qos",		  required_argument, 0, 'q'},
		{"quiet",         no_argument,       0, 'Q'},
		{"share",         no_argument,       0, 's'},
		{"core-spec",     required_argument, 0, 'S'},
		{"time",          required_argument, 0, 't'},
		{"usage",         no_argument,       0, 'u'},
		{"verbose",       no_argument,       0, 'v'},
		{"version",       no_argument,       0, 'V'},
		{"nodelist",      required_argument, 0, 'w'},
		{"wait",          required_argument, 0, 'W'},
		{"exclude",       required_argument, 0, 'x'},
		{"acctg-freq",    required_argument, 0, LONG_OPT_ACCTG_FREQ},
		{"begin",         required_argument, 0, LONG_OPT_BEGIN},
		{"bb",            required_argument, 0, LONG_OPT_BURST_BUFFER_SPEC},
		{"bbf",           required_argument, 0, LONG_OPT_BURST_BUFFER_FILE},
		{"bell",          no_argument,       0, LONG_OPT_BELL},
		{"comment",       required_argument, 0, LONG_OPT_COMMENT},
		{"contiguous",    no_argument,       0, LONG_OPT_CONT},
		{"cores-per-socket", required_argument, 0, LONG_OPT_CORESPERSOCKET},
		{"cpu-freq",         required_argument, 0, LONG_OPT_CPU_FREQ},
		{"cpus-per-gpu",  required_argument, 0, LONG_OPT_CPUS_PER_GPU},
		{"deadline",      required_argument, 0, LONG_OPT_DEADLINE},
		{"delay-boot",    required_argument, 0, LONG_OPT_DELAY_BOOT},
		{"exclusive",     optional_argument, 0, LONG_OPT_EXCLUSIVE},
		{"get-user-env",  optional_argument, 0, LONG_OPT_GET_USER_ENV},
		{"gid",           required_argument, 0, LONG_OPT_GID},
		{"gpu-bind",      required_argument, 0, LONG_OPT_GPU_BIND},
		{"gpu-freq",      required_argument, 0, LONG_OPT_GPU_FREQ},
		{"gpus-per-node", required_argument, 0, LONG_OPT_GPUS_PER_NODE},
		{"gpus-per-socket", required_argument, 0, LONG_OPT_GPUS_PER_SOCKET},
		{"gpus-per-task", required_argument, 0, LONG_OPT_GPUS_PER_TASK},
		{"gres",          required_argument, 0, LONG_OPT_GRES},
		{"gres-flags",    required_argument, 0, LONG_OPT_GRES_FLAGS},
		{"hint",          required_argument, 0, LONG_OPT_HINT},
		{"jobid",         required_argument, 0, LONG_OPT_JOBID},
		{"mail-type",     required_argument, 0, LONG_OPT_MAIL_TYPE},
		{"mail-user",     required_argument, 0, LONG_OPT_MAIL_USER},
		{"mcs-label",     required_argument, 0, LONG_OPT_MCS_LABEL},
		{"mem",           required_argument, 0, LONG_OPT_MEM},
		{"mem-per-cpu",   required_argument, 0, LONG_OPT_MEM_PER_CPU},
		{"mem-per-gpu",   required_argument, 0, LONG_OPT_MEM_PER_GPU},
		{"mem-bind",      required_argument, 0, LONG_OPT_MEM_BIND},
		{"mem_bind",      required_argument, 0, LONG_OPT_MEM_BIND},
		{"mincores",      required_argument, 0, LONG_OPT_MINCORES},
		{"mincpus",       required_argument, 0, LONG_OPT_MINCPU},
		{"minsockets",    required_argument, 0, LONG_OPT_MINSOCKETS},
		{"minthreads",    required_argument, 0, LONG_OPT_MINTHREADS},
		{"network",       required_argument, 0, LONG_OPT_NETWORK},
		{"nice",          optional_argument, 0, LONG_OPT_NICE},
		{"priority",      required_argument, 0, LONG_OPT_PRIORITY},
		{"no-bell",       no_argument,       0, LONG_OPT_NO_BELL},
		{"no-shell",      no_argument,       0, LONG_OPT_NOSHELL},
		{"ntasks-per-core",  required_argument, 0, LONG_OPT_NTASKSPERCORE},
		{"ntasks-per-node",  required_argument, 0, LONG_OPT_NTASKSPERNODE},
		{"ntasks-per-socket",required_argument, 0, LONG_OPT_NTASKSPERSOCKET},
		{"power",         required_argument, 0, LONG_OPT_POWER},
		{"profile",       required_argument, 0, LONG_OPT_PROFILE},
		{"reboot",	  no_argument,       0, LONG_OPT_REBOOT},
		{"reservation",   required_argument, 0, LONG_OPT_RESERVATION},
		{"signal",        required_argument, 0, LONG_OPT_SIGNAL},
		{"sockets-per-node", required_argument, 0, LONG_OPT_SOCKETSPERNODE},
		{"spread-job",    no_argument,       0, LONG_OPT_SPREAD_JOB},
		{"switches",      required_argument, 0, LONG_OPT_REQ_SWITCH},
		{"tasks-per-node",  required_argument, 0, LONG_OPT_NTASKSPERNODE},
		{"thread-spec",   required_argument, 0, LONG_OPT_THREAD_SPEC},
		{"time-min",      required_argument, 0, LONG_OPT_TIME_MIN},
		{"threads-per-core", required_argument, 0, LONG_OPT_THREADSPERCORE},
		{"tmp",           required_argument, 0, LONG_OPT_TMP},
		{"uid",           required_argument, 0, LONG_OPT_UID},
		{"use-min-nodes", no_argument,       0, LONG_OPT_USE_MIN_NODES},
		{"wait-all-nodes",required_argument, 0, LONG_OPT_WAIT_ALL_NODES},
		{"wckey",         required_argument, 0, LONG_OPT_WCKEY},
#ifdef WITH_SLURM_X11
		{"x11",           optional_argument, 0, LONG_OPT_X11},
#endif
		{NULL,            0,                 0, 0}
	};
	char *opt_string =
		"+A:B:c:C:d:D:F:G:hHI::J:k::K::L:m:M:n:N:Op:P:q:QsS:t:uU:vVw:W:x:";
	char *pos_delimit;

	struct option *optz = spank_option_table_create(long_options);

	if (!optz) {
		error("Unable to create options table");
		exit(error_exit);
	}

	opt.progname = xbasename(argv[0]);
	optind = 0;
	while ((opt_char = getopt_long(argc, argv, opt_string,
				      optz, &option_index)) != -1) {
		switch (opt_char) {
		case '?':
			fprintf(stderr, "Try \"salloc --help\" for more "
				"information\n");
			exit(error_exit);
			break;
		case 'A':
		case 'U':	/* backwards compatibility */
			arg_set_account(&opt, optarg, "--account", false);
			break;
		case 'B':
			arg_set_extra_node_info(&opt, optarg, "--extra-node-info", false);
			break;
		case 'c':
			arg_set_cpus_per_task(&oprt, optarg, "--cpus-per-task", true);
			break;
		case 'C':
			arg_set_constraint(&opt, optarg, "--constraint", false);
			break;
		case 'd':
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case 'D':
			arg_set_chdir(&opt, optarg, "--chdir", false);
			break;
		case 'F':
			arg_set_nodefile(&opt, optarg, "--nodefile", true);
			break;
		case 'G':
			arg_set_gpus(&opt, optarg, "--gpus", false);
			break;
		case 'h':
			_help();
			exit(0);
		case 'H':
			arg_set_hold(&opt, NULL, "--hold", false);
			break;
		case 'I':
			arg_set_immediate(&opt, optarg, "--immediate", false);
			break;
		case 'J':
			arg_set_job_name(&opt, optarg, "--job-name", false);
			break;
		case 'k':
			arg_set_no_kill(&opt, optarg, "--no-kill", false);
			break;
		case 'K': /* argument is optional */
			arg_set_kill_command(&opt, optarg, "--kill", false);
			break;
		case 'L':
			arg_set_licenses(&opt, optarg, "--licenses", false);
			break;
		case 'm':
			arg_set_distribution(&opt, optarg, "--distribution",
					     false);
			break;
		case 'M':
			arg_set_clusters(&opt, optarg, "--clusters", false);
			break;
		case 'n':
			arg_set_ntasks(&opt, optarg, "--ntasks", false);
			break;
		case 'N':
			arg_set_nodes(&opt, optarg, "--nodes", true);
			break;
		case 'O':
			arg_set_overcommit(&opt, NULL, "--overcommit", false);
			break;
		case 'p':
			arg_set_partition(&opt, optarg, "--partition", false);
			break;
		case 'P':
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case 'q':
			arg_set_qos(&opt, optarg, "--qos", false);
			break;
		case 'Q':
			arg_set_quiet(&opt, NULL, "--quiet", false);
			break;
		case 's':
			arg_set_share(&opt, NULL, "--share", false);
			break;
		case 'S':
			arg_set_core_spec(&opt, optarg, "--core-spec", false);
			break;
		case 't':
			arg_set_time(&opt, optarg, "--time", true);
			break;
		case 'u':
			_usage();
			exit(0);
		case 'v':
			arg_set_verbose(&opt, NULL, "--verbose", false);
			break;
		case 'V':
			print_slurm_version();
			exit(0);
			break;
		case 'w':
			arg_set_nodelist(&opt, optarg, "--nodelist", false);
			break;
		case 'W':
			arg_set_wait(&opt, optarg, "--wait", false);
			break;
		case 'x':
			arg_set_exclude(&opt, optarg, "--exclude", true);
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
		case LONG_OPT_DELAY_BOOT:
			arg_set_delay_boot(&opt, optarg, "--delay-boot", true);
			break;
		case LONG_OPT_EXCLUSIVE:
			arg_set_exclusive(&opt, optarg, "--exclusive", true);
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
		case LONG_OPT_MINCPU:
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
		case LONG_OPT_TMP:
			arg_set_tmp(&opt, optarg, "--tmp", true);
			break;
		case LONG_OPT_UID:
			arg_set_uid(&opt, optarg, "--uid", true);
			break;
		case LONG_OPT_GID:
			arg_set_gid(&opt, optarg, "--gid", true);
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
		case LONG_OPT_NICE:
			arg_set_nice(&opt, optarg, "--nice", false);
			break;
		case LONG_OPT_PRIORITY:
			arg_set_priority(&opt, optarg, "--priority", true);
			break;
		case LONG_OPT_BELL:
			arg_set_bell(&opt, NULL, "--bell", false);
			break;
		case LONG_OPT_NO_BELL:
			arg_set_no_bell(&opt, NULL, "--no-bell", false);
			break;
		case LONG_OPT_JOBID:
			arg_set_jobid(&opt, optarg, "--jobid", false);
			break;
		case LONG_OPT_PROFILE:
			arg_set_profile(&opt, optarg, "--profile", false);
			break;
		case LONG_OPT_COMMENT:
			arg_set_comment(&opt, optarg, "--comment", false);
			break;
		case LONG_OPT_SOCKETSPERNODE:
			arg_set_sockets_per_node(&opt, optarg, "--sockets-per-node", true);
			break;
		case LONG_OPT_CORESPERSOCKET:
			arg_set_cores_per_socket(&opt, optarg, "--cores-per-socket", true);
			break;
		case LONG_OPT_THREADSPERCORE:
			arg_set_threads_per_core(&opt, optarg, "--threads-per-core", true);
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
			arg_set_hint(&opt, optarg, "--hint", false);
			break;
		case LONG_OPT_REBOOT:
			arg_set_reboot(&opt, NULL, "--reboot", false);
			break;
		case LONG_OPT_ACCTG_FREQ:
			arg_set_acctg_freq(&opt, optarg, "--acctg-freq", false);
			break;
		case LONG_OPT_NOSHELL:
			arg_set_no_shell(&opt, NULL, "--no-shell", false);
			break;
		case LONG_OPT_GET_USER_ENV:
			arg_set_get_user_env(&opt, optarg, "--get-user-env", false);
			break;
		case LONG_OPT_NETWORK:
			arg_set_network(&opt, optarg, "--network", false);
			break;
		case LONG_OPT_MEM_BIND:
			arg_set_mem_bind(&opt, optarg, "--mem-bind", false);
			break;
		case LONG_OPT_WCKEY:
			arg_set_wckey(&opt, optarg, "--wckey", false);
			break;
		case LONG_OPT_RESERVATION:
			arg_set_reservation(&opt, optarg, "--reservation", false);
			break;
		case LONG_OPT_POWER:
			arg_set_power(&opt, optarg, "--power", false);
			break;
		case LONG_OPT_SIGNAL:
			arg_set_signal(&opt, optarg, "--signal", true);
			break;
		case LONG_OPT_TIME_MIN:
			arg_set_time_min(&opt, optarg, "--time-min", true);
			break;
		case LONG_OPT_GRES:
			arg_set_gres(&opt, optarg, "--gres", false);
			break;
		case LONG_OPT_GRES_FLAGS:
			arg_set_gres_flags(&opt, optarg, "--gres-flags", true);
			break;
		case LONG_OPT_WAIT_ALL_NODES:
			arg_set_wait_all_nodes(&opt, optarg, "--wait-all-nodes", true);
			break;
		case LONG_OPT_CPU_FREQ:
			arg_set_cpu_freq(&opt, optarg, "--cpu-freq", false);
			break;
		case LONG_OPT_REQ_SWITCH:
			arg_set_switches(&opt, optarg, "--switches", false);
			break;
		case LONG_OPT_BURST_BUFFER_SPEC:
			arg_set_bb(&opt, optarg, "--bb", false);
			break;
		case LONG_OPT_BURST_BUFFER_FILE:
			arg_set_bbf(&opt, optarg, "--bbf", false);
			break;
		case LONG_OPT_THREAD_SPEC:
			arg_set_thread_spec(&opt, optarg, "--thread-spec", false);
			break;
		case LONG_OPT_SPREAD_JOB:
			arg_set_spread_job(&opt, NULL, "--spread-job", false);
			break;
		case LONG_OPT_USE_MIN_NODES:
			arg_set_use_min_nodes(&opt, NULL, "--use-min-nodes", false);
			break;
		case LONG_OPT_X11:
			arg_set_x11(&opt, optarg, "--x11", false);
			break;
		default:
			if (spank_process_option(opt_char, optarg) < 0)
				exit(error_exit);
		}
	}

	spank_option_table_destroy(optz);
}

/*
 * _opt_args() : set options via commandline args and popt
 */
static void _opt_args(int argc, char **argv)
{
	int i;
	char **rest = NULL;

	_set_options(argc, argv);

	if ((optind < argc) && !xstrcmp(argv[optind], ":")) {
		debug("pack job separator");
	} else {
		command_argc = 0;
		if (optind < argc) {
			rest = argv + optind;
			while (rest[command_argc] != NULL)
				command_argc++;
		}
		command_argv = (char **) xmalloc((command_argc + 1) *
						 sizeof(char *));
		for (i = 0; i < command_argc; i++) {
			if ((i == 0) && (rest == NULL))
				break;	/* Fix for CLANG false positive */
			command_argv[i] = xstrdup(rest[i]);
		}
		command_argv[i] = NULL;	/* End of argv's (for possible execv) */
	}

}

/* _get_shell - return a string containing the default shell for this user
 * NOTE: This function is NOT reentrant (see getpwuid_r if needed) */
static char *_get_shell(void)
{
	struct passwd *pw_ent_ptr;

	pw_ent_ptr = getpwuid(opt.uid);
	if (!pw_ent_ptr) {
		pw_ent_ptr = getpwnam("nobody");
		error("warning - no user information for user %d", opt.uid);
	}
	return pw_ent_ptr->pw_shell;
}

static int _salloc_default_command(int *argcp, char **argvp[])
{
	slurm_ctl_conf_t *cf = slurm_conf_lock();

	if (cf->salloc_default_command) {
		/*
		 *  Set argv to "/bin/sh -c 'salloc_default_command'"
		 */
		*argcp = 3;
		*argvp = xmalloc(sizeof (char *) * 4);
		(*argvp)[0] = "/bin/sh";
		(*argvp)[1] = "-c";
		(*argvp)[2] = xstrdup (cf->salloc_default_command);
		(*argvp)[3] = NULL;
	} else {
		*argcp = 1;
		*argvp = xmalloc(sizeof (char *) * 2);
		(*argvp)[0] = _get_shell();
		(*argvp)[1] = NULL;
	}

	slurm_conf_unlock();
	return (0);
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

	if (opt.quiet && opt.verbose) {
		error ("don't specify both --verbose (-v) and --quiet (-Q)");
		verified = false;
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
		if (!_valid_node_list(&opt.nodelist))
			exit(error_exit);
	}

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

	if ((opt.ntasks_per_node > 0) && (!opt.ntasks_set)) {
		opt.ntasks = opt.min_nodes * opt.ntasks_per_node;
		opt.ntasks_set = 1;
	}

	if (opt.cpus_set && (opt.pn_min_cpus < opt.cpus_per_task))
		opt.pn_min_cpus = opt.cpus_per_task;

	if ((opt.euid != (uid_t) -1) && (opt.euid != opt.uid))
		opt.uid = opt.euid;

	if ((opt.egid != (gid_t) -1) && (opt.egid != opt.gid))
		opt.gid = opt.egid;

	if ((saopt.no_shell == false) && (command_argc == 0)) {
		_salloc_default_command(&command_argc, &command_argv);
		if (!opt.job_name)
			saopt.default_job_name = true;
	}

	if ((opt.job_name == NULL) && (command_argc > 0))
		opt.job_name = base_name(command_argv[0]);

	/* check for realistic arguments */
	if (opt.ntasks <= 0) {
		error("invalid number of tasks (-n %d)",
		      opt.ntasks);
		verified = false;
	}

	if ((opt.min_nodes < 0) || (opt.max_nodes < 0) ||
	    (opt.max_nodes && (opt.min_nodes > opt.max_nodes))) {
		error("invalid number of nodes (-N %d-%d)",
		      opt.min_nodes, opt.max_nodes);
		verified = false;
	}

	if ((opt.pn_min_memory > -1) && (opt.mem_per_cpu > -1)) {
		if (opt.pn_min_memory < opt.mem_per_cpu) {
			info("mem < mem-per-cpu - resizing mem to be equal "
			     "to mem-per-cpu");
			opt.pn_min_memory = opt.mem_per_cpu;
		}
	}

        /* Check to see if user has specified enough resources to
	 * satisfy the plane distribution with the specified
	 * plane_size.
	 * if (n/plane_size < N) and ((N-1) * plane_size >= n) -->
	 * problem Simple check will not catch all the problem/invalid
	 * cases.
	 * The limitations of the plane distribution in the cons_res
	 * environment are more extensive and are documented in the
	 * Slurm reference guide.  */
	if ((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_PLANE &&
	    opt.plane_size) {
		if ((opt.ntasks/opt.plane_size) < opt.min_nodes) {
			if (((opt.min_nodes-1)*opt.plane_size) >= opt.ntasks) {
#if (0)
				info("Too few processes ((n/plane_size) %d < N %d) "
				     "and ((N-1)*(plane_size) %d >= n %d)) ",
				     opt.ntasks/opt.plane_size, opt.min_nodes,
				     (opt.min_nodes-1)*opt.plane_size,
				     opt.ntasks);
#endif
				error("Too few processes for the requested "
				      "{plane,node} distribution");
				exit(error_exit);
			}
		}
	}

	/* massage the numbers */
	if ((opt.nodes_set || opt.extra_set)				&&
	    ((opt.min_nodes == opt.max_nodes) || (opt.max_nodes == 0))	&&
	    !opt.ntasks_set) {
		/* 1 proc / node default */
		opt.ntasks = opt.min_nodes;

		/* 1 proc / min_[socket * core * thread] default */
		if (opt.sockets_per_node != NO_VAL) {
			opt.ntasks *= opt.sockets_per_node;
			opt.ntasks_set = true;
		}
		if (opt.cores_per_socket != NO_VAL) {
			opt.ntasks *= opt.cores_per_socket;
			opt.ntasks_set = true;
		}
		if (opt.threads_per_core != NO_VAL) {
			opt.ntasks *= opt.threads_per_core;
			opt.ntasks_set = true;
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
		if (opt.ntasks < opt.min_nodes) {

			info ("Warning: can't run %d processes on %d "
			      "nodes, setting nnodes to %d",
			      opt.ntasks, opt.min_nodes, opt.ntasks);

			opt.min_nodes = opt.max_nodes = opt.ntasks;

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

	} /* else if (opt.ntasks_set && !opt.nodes_set) */

	/* set up the proc and node counts based on the arbitrary list
	   of nodes */
	if (((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_ARBITRARY)
	    && (!opt.nodes_set || !opt.ntasks_set)) {
		if (!hl)
			hl = hostlist_create(opt.nodelist);
		if (!opt.ntasks_set) {
			opt.ntasks_set = 1;
			opt.ntasks = hostlist_count(hl);
		}
		if (!opt.nodes_set) {
			opt.nodes_set = 1;
			hostlist_uniq(hl);
			opt.min_nodes = opt.max_nodes = hostlist_count(hl);
		}
	}

	if (hl)
		hostlist_destroy(hl);

	if ((opt.deadline) && (opt.begin) && (opt.deadline < opt.begin)) {
		error("Incompatible begin and deadline time specification");
		exit(error_exit);
	}

#ifdef HAVE_NATIVE_CRAY
	if (opt.network && opt.shared)
		fatal("Requesting network performance counters requires "
		      "exclusive access.  Please add the --exclusive option "
		      "to your request.");
#endif

	if (opt.mem_bind_type && (getenv("SLURM_MEM_BIND") == NULL)) {
		char tmp[64];
		slurm_sprint_mem_bind_type(tmp, opt.mem_bind_type);
		if (opt.mem_bind) {
			setenvf(NULL, "SLURM_MEM_BIND", "%s:%s",
				tmp, opt.mem_bind);
		} else {
			setenvf(NULL, "SLURM_MEM_BIND", "%s", tmp);
		}
	}
	if (opt.mem_bind_type && (getenv("SLURM_MEM_BIND_SORT") == NULL) &&
	    (opt.mem_bind_type & MEM_BIND_SORT)) {
		setenvf(NULL, "SLURM_MEM_BIND_SORT", "sort");
	}

	if (opt.mem_bind_type && (getenv("SLURM_MEM_BIND_VERBOSE") == NULL)) {
		if (opt.mem_bind_type & MEM_BIND_VERBOSE) {
			setenvf(NULL, "SLURM_MEM_BIND_VERBOSE", "verbose");
		} else {
			setenvf(NULL, "SLURM_MEM_BIND_VERBOSE", "quiet");
		}
	}

	if ((opt.ntasks_per_node > 0) &&
	    (getenv("SLURM_NTASKS_PER_NODE") == NULL)) {
		setenvf(NULL, "SLURM_NTASKS_PER_NODE", "%d",
			opt.ntasks_per_node);
	}

	if (opt.profile)
		setenvfs("SLURM_PROFILE=%s",
			 acct_gather_profile_to_string(opt.profile));

	cpu_freq_set_env("SLURM_CPU_FREQ_REQ",
			opt.cpu_freq_min, opt.cpu_freq_max, opt.cpu_freq_gov);

	if (saopt.wait_all_nodes == NO_VAL16) {
		char *sched_params;
		sched_params = slurm_get_sched_params();
		if (sched_params && xstrcasestr(sched_params,
						"salloc_wait_nodes"))
			saopt.wait_all_nodes = 1;
		xfree(sched_params);
	}

	if (opt.x11) {
		opt.x11_target_port = x11_get_display_port();
		opt.x11_magic_cookie = x11_get_xauth();
	}

	return verified;
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

	for (i=0; i<opt.spank_job_env_size; i++) {
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

	for (i=0; i<opt.spank_job_env_size; i++) {
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

	for (i=0; i<opt.spank_job_env_size; i++) {
		if (xstrncmp(opt.spank_job_env[i], tmp_str, len))
			continue;
		xfree(opt.spank_job_env[i]);
		for (j=(i+1); j<opt.spank_job_env_size; i++, j++)
			opt.spank_job_env[i] = opt.spank_job_env[j];
		opt.spank_job_env_size--;
		if (opt.spank_job_env_size == 0)
			xfree(opt.spank_job_env);
		return 0;
	}

	return 0;	/* not found */
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

/* helper function for printing options
 *
 * warning: returns pointer to memory allocated on the stack.
 */
static char *print_constraints()
{
	char *buf = xstrdup("");

	if (opt.pn_min_cpus > 0)
		xstrfmtcat(buf, "mincpus=%d ", opt.pn_min_cpus);

	if (opt.pn_min_memory > 0)
		xstrfmtcat(buf, "mem=%"PRIi64"M ", opt.pn_min_memory);

	if (opt.mem_per_cpu > 0)
		xstrfmtcat(buf, "mem-per-cpu=%"PRIi64"M ", opt.mem_per_cpu);

	if (opt.pn_min_tmp_disk > 0)
		xstrfmtcat(buf, "tmp=%ld ", opt.pn_min_tmp_disk);

	if (opt.contiguous == true)
		xstrcat(buf, "contiguous ");

	if (opt.nodelist != NULL)
		xstrfmtcat(buf, "nodelist=%s ", opt.nodelist);

	if (opt.exc_nodes != NULL)
		xstrfmtcat(buf, "exclude=%s ", opt.exc_nodes);

	if (opt.constraints != NULL)
		xstrfmtcat(buf, "constraints=`%s' ", opt.constraints);

	if (opt.c_constraints != NULL)
		xstrfmtcat(buf, "cluster-constraints=`%s' ", opt.c_constraints);

	return buf;
}

#define tf_(b) (b == true) ? "true" : "false"

static void _opt_list(void)
{
	char *str;

	info("defined options for program `%s'", opt.progname);
	info("--------------- ---------------------");

	info("user           : `%s'", opt.user);
	info("uid            : %u", (uint32_t) opt.uid);
	info("gid            : %u", (uint32_t) opt.gid);
	info("ntasks         : %d %s", opt.ntasks,
		opt.ntasks_set ? "(set)" : "(default)");
	info("cpus_per_task  : %d %s", opt.cpus_per_task,
		opt.cpus_set ? "(set)" : "(default)");
	if (opt.max_nodes)
		info("nodes          : %d-%d", opt.min_nodes, opt.max_nodes);
	else {
		info("nodes          : %d %s", opt.min_nodes,
			opt.nodes_set ? "(set)" : "(default)");
	}
	info("partition      : %s",
		opt.partition == NULL ? "default" : opt.partition);
	info("job name       : `%s'", opt.job_name);
	info("reservation    : `%s'", opt.reservation);
	info("wckey          : `%s'", opt.wckey);
	if (opt.jobid != NO_VAL)
		info("jobid          : %u", opt.jobid);
	if (opt.delay_boot != NO_VAL)
		info("delay_boot     : %u", opt.delay_boot);
	info("distribution   : %s", format_task_dist_states(opt.distribution));
	if (opt.distribution == SLURM_DIST_PLANE)
		info("plane size   : %u", opt.plane_size);
	info("verbose        : %d", opt.verbose);
	if (opt.immediate <= 1)
		info("immediate      : %s", tf_(opt.immediate));
	else
		info("immediate      : %d secs", (opt.immediate - 1));
	info("overcommit     : %s", tf_(opt.overcommit));
	if (opt.time_limit == INFINITE)
		info("time_limit     : INFINITE");
	else if (opt.time_limit != NO_VAL)
		info("time_limit     : %d", opt.time_limit);
	if (opt.time_min != NO_VAL)
		info("time_min       : %d", opt.time_min);
	if (opt.nice)
		info("nice           : %d", opt.nice);
	info("account        : %s", opt.account);
	info("comment        : %s", opt.comment);
	info("dependency     : %s", opt.dependency);
	if (opt.gres != NULL)
		info("gres           : %s", opt.gres);
	info("network        : %s", opt.network);
	info("power          : %s", power_flags_str(opt.power_flags));
	info("profile        : `%s'",
	     acct_gather_profile_to_string(opt.profile));
	info("qos            : %s", opt.qos);
	str = print_constraints();
	info("constraints    : %s", str);
	xfree(str);
	info("reboot         : %s", opt.reboot ? "no" : "yes");

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
	info("mail_type      : %s", print_mail_type(opt.mail_type));
	info("mail_user      : %s", opt.mail_user);
	info("sockets-per-node  : %d", opt.sockets_per_node);
	info("cores-per-socket  : %d", opt.cores_per_socket);
	info("threads-per-core  : %d", opt.threads_per_core);
	info("ntasks-per-node   : %d", opt.ntasks_per_node);
	info("ntasks-per-socket : %d", opt.ntasks_per_socket);
	info("ntasks-per-core   : %d", opt.ntasks_per_core);
	info("plane_size        : %u", opt.plane_size);
	info("mem-bind          : %s",
	     opt.mem_bind == NULL ? "default" : opt.mem_bind);
	str = print_commandline(command_argc, command_argv);
	info("user command   : `%s'", str);
	xfree(str);
	info("cpu_freq_min   : %u", opt.cpu_freq_min);
	info("cpu_freq_max   : %u", opt.cpu_freq_max);
	info("cpu_freq_gov   : %u", opt.cpu_freq_gov);
	info("switches          : %d", opt.req_switch);
	info("wait-for-switches : %d", opt.wait4switch);
	if (opt.core_spec == NO_VAL16)
		info("core-spec         : NA");
	else if (opt.core_spec & CORE_SPEC_THREAD) {
		info("thread-spec       : %d",
		     opt.core_spec & (~CORE_SPEC_THREAD));
	} else
		info("core-spec         : %d", opt.core_spec);
	info("burst_buffer      : `%s'", opt.burst_buffer);
	if (opt.mcs_label)
		info("mcs-label         : %s",opt.mcs_label);
	info("cpus-per-gpu      : %d", opt.cpus_per_gpu);
	info("gpus              : %s", opt.gpus);
	info("gpu-bind          : %s", opt.gpu_bind);
	info("gpu-freq          : %s", opt.gpu_freq);
	info("gpus-per-node     : %s", opt.gpus_per_node);
	info("gpus-per-socket   : %s", opt.gpus_per_socket);
	info("gpus-per-task     : %s", opt.gpus_per_task);
	info("mem-per-gpu       : %"PRIi64, opt.mem_per_gpu);
}

static void _usage(void)
{
 	printf(
"Usage: salloc [-N numnodes|[min nodes]-[max nodes]] [-n num-processors]\n"
"              [[-c cpus-per-node] [-r n] [-p partition] [--hold] [-t minutes]\n"
"              [--immediate[=secs]] [--no-kill] [--overcommit] [-D path]\n"
"              [--oversubscribe] [-J jobname] [--jobid=id]\n"
"              [--verbose] [--gid=group] [--uid=user] [--licenses=names]\n"
"              [--clusters=cluster_names]\n"
"              [--contiguous] [--mincpus=n] [--mem=MB] [--tmp=MB] [-C list]\n"
"              [--account=name] [--dependency=type:jobid] [--comment=name]\n"
"              [--mail-type=type] [--mail-user=user] [--nice[=value]]\n"
"              [--bell] [--no-bell] [--kill-command[=signal]] [--spread-job]\n"
"              [--nodefile=file] [--nodelist=hosts] [--exclude=hosts]\n"
"              [--network=type] [--mem-per-cpu=MB] [--qos=qos]\n"
"              [--mem-bind=...] [--reservation=name] [--mcs-label=mcs]\n"
"              [--time-min=minutes] [--gres=list] [--gres-flags=opts]\n"
"              [--cpu-freq=min[-max[:gov]] [--power=flags] [--profile=...]\n"
"              [--switches=max-switches[@max-time-to-wait]]\n"
"              [--core-spec=cores] [--thread-spec=threads] [--reboot]\n"
"              [--bb=burst_buffer_spec] [--bbf=burst_buffer_file]\n"
"              [--delay-boot=mins] [--use-min-nodes]\n"
"              [--cpus-per-gpu=n] [--gpus=n] [--gpu-bind=...] [--gpu-freq=...]\n"
"              [--gpus-per-node=n] [--gpus-per-socket=n]  [--gpus-per-task=n]\n"
"              [--mem-per-gpu=MB]\n"
"              [executable [args...]]\n");
}

static void _help(void)
{
	slurm_ctl_conf_t *conf;

        printf (
"Usage: salloc [OPTIONS...] [executable [args...]]\n"
"\n"
"Parallel run options:\n"
"  -A, --account=name          charge job to specified account\n"
"      --begin=time            defer job until HH:MM MM/DD/YY\n"
"      --bell                  ring the terminal bell when the job is allocated\n"
"      --bb=<spec>             burst buffer specifications\n"
"      --bbf=<file_name>       burst buffer specification file\n"
"  -c, --cpus-per-task=ncpus   number of cpus required per task\n"
"      --comment=name          arbitrary comment\n"
"      --cpu-freq=min[-max[:gov]] requested cpu frequency (and governor)\n"
"      --delay-boot=mins       delay boot for desired node features\n"
"  -d, --dependency=type:jobid defer job until condition on jobid is satisfied\n"
"      --deadline=time         remove the job if no ending possible before\n"
"                              this deadline (start > (deadline - time[-min]))\n"
"  -D, --chdir=path            change working directory\n"
"      --get-user-env          used by Moab.  See srun man page.\n"
"      --gid=group_id          group ID to run job as (user root only)\n"
"      --gres=list             required generic resources\n"
"      --gres-flags=opts       flags related to GRES management\n"
"  -H, --hold                  submit job in held state\n"
"  -I, --immediate[=secs]      exit if resources not available in \"secs\"\n"
"      --jobid=id              specify jobid to use\n"
"  -J, --job-name=jobname      name of job\n"
"  -k, --no-kill               do not kill job on node failure\n"
"  -K, --kill-command[=signal] signal to send terminating job\n"
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
"  -n, --tasks=N               number of processors required\n"
"      --nice[=value]          decrease scheduling priority by value\n"
"      --no-bell               do NOT ring the terminal bell\n"
"      --ntasks-per-node=n     number of tasks to invoke on each node\n"
"  -N, --nodes=N               number of nodes on which to run (N = min[-max])\n"
"  -O, --overcommit            overcommit resources\n"
"      --power=flags           power management options\n"
"      --priority=value        set the priority of the job to value\n"
"      --profile=value         enable acct_gather_profile for detailed data\n"
"                              value is all or none or any combination of\n"
"                              energy, lustre, network or task\n"
"  -p, --partition=partition   partition requested\n"
"  -q, --qos=qos               quality of service\n"
"  -Q, --quiet                 quiet mode (suppress informational messages)\n"
"      --reboot                reboot compute nodes before starting job\n"
"  -s, --oversubscribe         oversubscribe resources with other jobs\n"
"      --signal=[B:]num[@time] send signal when time limit within time seconds\n"
"      --spread-job            spread job across as many nodes as possible\n"
"      --switches=max-switches{@max-time-to-wait}\n"
"                              Optimum switches and max time to wait for optimum\n"
"  -S, --core-spec=cores       count of reserved cores\n"
"      --thread-spec=threads   count of reserved threads\n"
"  -t, --time=minutes          time limit\n"
"      --time-min=minutes      minimum time limit (if distinct)\n"
"      --uid=user_id           user ID to run job as (user root only)\n"
"      --use-min-nodes         if a range of node counts is given, prefer the\n"
"                              smaller count\n"
"  -v, --verbose               verbose mode (multiple -v's increase verbosity)\n"
"      --wckey=wckey           wckey to run job under\n"
"\n"
"Constraint options:\n"
"      --cluster-constraint=list specify a list of cluster constraints\n"
"      --contiguous            demand a contiguous range of nodes\n"
"  -C, --constraint=list       specify a list of constraints\n"
"  -F, --nodefile=filename     request a specific list of hosts\n"
"      --mem=MB                minimum amount of real memory\n"
"      --mincpus=n             minimum number of logical processors (threads)\n"
"                              per node\n"
"      --reservation=name      allocate resources from named reservation\n"
"      --tmp=MB                minimum amount of temporary disk\n"
"  -w, --nodelist=hosts...     request a specific list of hosts\n"
"  -x, --exclude=hosts...      exclude a specific list of hosts\n"
"\n"
"Consumable resources related options:\n"
"      --exclusive[=user]      allocate nodes in exclusive mode when\n"
"                              cpu consumable resource is enabled\n"
"      --exclusive[=mcs]       allocate nodes in exclusive mode when\n"
"                              cpu consumable resource is enabled\n"
"                              and mcs plugin is enabled\n"
"      --mem-per-cpu=MB        maximum amount of real memory per allocated\n"
"                              cpu required by the job.\n"
"                              --mem >= --mem-per-cpu if --mem is specified.\n"
"\n"
"Affinity/Multi-core options: (when the task/affinity plugin is enabled)\n"
"  -B  --extra-node-info=S[:C[:T]]            Expands to:\n"
"       --sockets-per-node=S   number of sockets per node to allocate\n"
"       --cores-per-socket=C   number of cores per socket to allocate\n"
"       --threads-per-core=T   number of threads per core to allocate\n"
"                              each field can be 'min' or wildcard '*'\n"
"                              total cpus requested = (N x S x C x T)\n"
"\n"
"      --ntasks-per-core=n     number of tasks to invoke on each core\n"
"      --ntasks-per-socket=n   number of tasks to invoke on each socket\n");
	conf = slurm_conf_lock();
	if (xstrstr(conf->task_plugin, "affinity")) {
		printf(
"      --hint=                 Bind tasks according to application hints\n"
"                              (see \"--hint=help\" for options)\n"
"      --mem-bind=             Bind memory to locality domains (ldom)\n"
"                              (see \"--mem-bind=help\" for options)\n");
	}
	slurm_conf_unlock();

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
	spank_print_options(stdout, 6, 30);

	printf("\n"
#ifdef HAVE_NATIVE_CRAY			/* Native Cray specific options */
"Cray related options:\n"
"      --network=type          Use network performance counters\n"
"                              (system, network, or processor)\n"
"\n"
#endif
"\n"
"Help options:\n"
"  -h, --help                  show this help message\n"
"  -u, --usage                 display brief usage message\n"
"\n"
"Other options:\n"
"  -V, --version               output version information and exit\n"
"\n"
		);
}
