/*****************************************************************************\
 *  opt.c - options processing for sbatch
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

#include <ctype.h>
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

#include "src/sbatch/opt.h"

enum wrappers {
	WRPR_START,
	WRPR_BSUB,
	WRPR_PBS,
	WRPR_CNT
};

/* generic OPT_ definitions -- mainly for use with env vars  */
#define OPT_NONE        0x00
#define OPT_INT         0x01
#define OPT_STRING      0x02
#define OPT_DEBUG       0x03
#define OPT_NODES       0x04
#define OPT_BOOL        0x05
#define OPT_CORE        0x06
#define OPT_DISTRIB	0x08
#define OPT_MULTI	0x0b
#define OPT_EXCLUSIVE	0x0c
#define OPT_OVERCOMMIT	0x0d
#define OPT_OPEN_MODE	0x0e
#define OPT_ACCTG_FREQ  0x0f
#define OPT_NO_REQUEUE  0x10
#define OPT_REQUEUE     0x11
#define OPT_THREAD_SPEC 0x12
#define OPT_MEM_BIND    0x13
#define OPT_WCKEY       0x14
#define OPT_SIGNAL      0x15
#define OPT_GET_USER_ENV  0x16
#define OPT_EXPORT        0x17
#define OPT_GRES_FLAGS    0x18
#define OPT_TIME_VAL      0x19
#define OPT_CORE_SPEC     0x1a
#define OPT_CPU_FREQ      0x1b
#define OPT_POWER         0x1d
#define OPT_SPREAD_JOB    0x1e
#define OPT_ARRAY_INX     0x20
#define OPT_PROFILE       0x21
#define OPT_HINT	  0x22
#define OPT_DELAY_BOOT	  0x23
#define OPT_INT64	  0x24
#define OPT_USE_MIN_NODES 0x25
#define OPT_MEM_PER_GPU   0x26
#define OPT_NO_KILL       0x27

/* generic getopt_long flags, integers and *not* valid characters */
#define LONG_OPT_PROPAGATE   0x100
#define LONG_OPT_BATCH       0x101
#define LONG_OPT_MEM_BIND    0x102
#define LONG_OPT_POWER       0x103
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
#define LONG_OPT_NO_REQUEUE  0x116
#define LONG_OPT_COMMENT     0x117
#define LONG_OPT_WRAP        0x118
#define LONG_OPT_REQUEUE     0x119
#define LONG_OPT_NETWORK     0x120
#define LONG_OPT_SOCKETSPERNODE  0x130
#define LONG_OPT_CORESPERSOCKET  0x131
#define LONG_OPT_THREADSPERCORE  0x132
#define LONG_OPT_NTASKSPERNODE   0x136
#define LONG_OPT_NTASKSPERSOCKET 0x137
#define LONG_OPT_NTASKSPERCORE   0x138
#define LONG_OPT_MEM_PER_CPU     0x13a
#define LONG_OPT_HINT            0x13b
#define LONG_OPT_REBOOT          0x144
#define LONG_OPT_GET_USER_ENV    0x146
#define LONG_OPT_OPEN_MODE       0x147
#define LONG_OPT_ACCTG_FREQ      0x148
#define LONG_OPT_WCKEY           0x149
#define LONG_OPT_RESERVATION     0x14a
#define LONG_OPT_CHECKPOINT      0x14b
#define LONG_OPT_CHECKPOINT_DIR  0x14c
#define LONG_OPT_SIGNAL          0x14d
#define LONG_OPT_TIME_MIN        0x14e
#define LONG_OPT_GRES            0x14f
#define LONG_OPT_WAIT_ALL_NODES  0x150
#define LONG_OPT_EXPORT          0x151
#define LONG_OPT_REQ_SWITCH      0x152
#define LONG_OPT_EXPORT_FILE     0x153
#define LONG_OPT_PROFILE         0x154
#define LONG_OPT_IGNORE_PBS      0x155
#define LONG_OPT_TEST_ONLY       0x156
#define LONG_OPT_PARSABLE        0x157
#define LONG_OPT_CPU_FREQ        0x158
#define LONG_OPT_THREAD_SPEC     0x159
#define LONG_OPT_GRES_FLAGS      0x15a
#define LONG_OPT_PRIORITY        0x160
#define LONG_OPT_KILL_INV_DEP    0x161
#define LONG_OPT_SPREAD_JOB      0x162
#define LONG_OPT_USE_MIN_NODES   0x163
#define LONG_OPT_MCS_LABEL       0x165
#define LONG_OPT_DEADLINE        0x166
#define LONG_OPT_BURST_BUFFER_FILE 0x167
#define LONG_OPT_DELAY_BOOT      0x168
#define LONG_OPT_CLUSTER_CONSTRAINT 0x169
#define LONG_OPT_X11             0x170
#define LONG_OPT_BURST_BUFFER_SPEC  0x171
#define LONG_OPT_CPUS_PER_GPU    0x172
#define LONG_OPT_GPU_BIND        0x173
#define LONG_OPT_GPU_FREQ        0x174
#define LONG_OPT_GPUS            0x175
#define LONG_OPT_GPUS_PER_NODE   0x176
#define LONG_OPT_GPUS_PER_SOCKET 0x177
#define LONG_OPT_GPUS_PER_TASK   0x178
#define LONG_OPT_MEM_PER_GPU     0x179

/*---- global variables, defined in opt.h ----*/
slurm_opt_t opt;
sbatch_opt_t sbopt;
sbatch_env_t pack_env;
int   error_exit = 1;
bool  is_pack_job = false;

/*---- forward declarations of static functions  ----*/

typedef struct env_vars env_vars_t;

static void  _help(void);

/* fill in default options  */
static void _opt_default(bool first_pass);

/* set options from batch script */
static bool _opt_batch_script(const char *file, const void *body, int size,
			      int pack_inx);

/* set options from pbs batch script */
static bool _opt_wrpr_batch_script(const char *file, const void *body, int size,
				   int argc, char **argv, int magic);

/* Wrapper functions */
static void _set_pbs_options(int argc, char **argv);
static void _set_bsub_options(int argc, char **argv);

/* set options based upon env vars  */
static void _opt_env(int pass);

/* list known options and their settings  */
static void  _opt_list(void);

/* verify options sanity  */
static bool _opt_verify(void);

static void _fullpath(char **filename, const char *cwd);
static void _parse_pbs_resource_list(char *rl);
static void _set_options(int argc, char **argv);
static void _usage(void);

/*---[ end forward declarations of static functions ]---------------------*/

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
		opt.progname ? opt.progname : "sbatch", buf);
	va_end(ap);
}
#else
#  define argerror error
#endif				/* USE_ARGERROR */

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
 * _opt_default(): used by initialize_and_process_args to set defaults
 */
static void _opt_default(bool first_pass)
{
	char buf[MAXPATHLEN + 1];
	uid_t uid = getuid();

	/* Some options will persist for all components of a heterogeneous job
	 * once specified for one, but will be overwritten with new values if
	 * specified on the command line */
	if (first_pass) {
		opt.salloc_opt = NULL;
		opt.sbatch_opt = &sbopt;
		opt.srun_opt = NULL;
		sbopt.pack_env = &pack_env;
		xfree(opt.account);
		xfree(opt.acctg_freq);
		opt.begin		= 0;
		xfree(opt.c_constraints);
		sbopt.ckpt_dir 		= slurm_get_checkpoint_dir();
		sbopt.ckpt_interval	= 0;
		xfree(sbopt.ckpt_interval_str);
		xfree(opt.clusters);
		opt.cpus_per_gpu	= 0;
		xfree(opt.comment);
		if ((getcwd(buf, MAXPATHLEN)) == NULL) {
			error("getcwd failed: %m");
			exit(error_exit);
		}
		opt.cwd			= xstrdup(buf);
		opt.deadline		= 0;
		opt.delay_boot		= NO_VAL;
		xfree(opt.dependency);
		opt.egid		= (gid_t) -1;
		xfree(sbopt.efname);
		xfree(opt.extra);
		xfree(opt.exc_nodes);
		xfree(sbopt.export_env);
		xfree(sbopt.export_file);
		opt.euid		= (uid_t) -1;
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
		sbopt.ifname		= xstrdup("/dev/null");
		opt.immediate		= false;
		xfree(opt.mcs_label);
		opt.mem_per_gpu		= 0;
		opt.nice		= NO_VAL;
		opt.no_kill		= false;
		xfree(sbopt.ofname);
		sbopt.parsable		= false;
		opt.priority		= 0;
		opt.profile		= ACCT_GATHER_PROFILE_NOT_SET;
		xfree(sbopt.propagate); 	 /* propagate specific rlimits */
		xfree(opt.qos);
		opt.quiet		= 0;
		opt.reboot		= false;
		sbopt.requeue		= NO_VAL;
		xfree(opt.reservation);
		sbopt.test_only		= false;
		opt.time_limit		= NO_VAL;
		opt.time_min		= NO_VAL;
		opt.uid			= uid;
		sbopt.umask		= -1;
		opt.user		= uid_to_string(uid);
		if (xstrcmp(opt.user, "nobody") == 0)
			fatal("Invalid user id: %u", uid);
		sbopt.wait		= false;
		sbopt.wait_all_nodes	= NO_VAL16;
		opt.warn_flags		= 0;
		opt.warn_signal		= 0;
		opt.warn_time		= 0;
		xfree(opt.wckey);
		opt.x11			= 0;
	}

	/* All other options must be specified individually for each component
	 * of the job */
	xfree(opt.burst_buffer);
	xfree(opt.constraints);
	opt.contiguous			= false;
	opt.core_spec			= NO_VAL16;
	opt.cores_per_socket		= NO_VAL; /* requested cores */
	opt.cpu_freq_gov		= NO_VAL;
	opt.cpu_freq_max		= NO_VAL;
	opt.cpu_freq_min		= NO_VAL;
	opt.cpus_per_task		= 0;
	opt.cpus_set			= false;
	opt.distribution		= SLURM_DIST_UNKNOWN;
	xfree(opt.gres);
	opt.hint_env			= NULL;
	opt.hint_set			= false;
	opt.job_flags			= 0;
	opt.jobid			= NO_VAL;
	opt.jobid_set			= false;
	opt.mail_type			= 0;
	xfree(opt.mail_user);
	opt.max_nodes			= 0;
	xfree(opt.mem_bind);
	opt.mem_bind_type		= 0;
	opt.mem_per_cpu			= -1;
	opt.pn_min_cpus			= -1;
	opt.min_nodes			= 1;
	xfree(opt.nodelist);
	opt.nodes_set			= false;
	opt.ntasks			= 1;
	opt.ntasks_per_core		= NO_VAL;
	opt.ntasks_per_core_set		= false;
	opt.ntasks_per_node		= 0;	/* ntask max limits */
	opt.ntasks_per_socket		= NO_VAL;
	opt.ntasks_set			= false;
	opt.overcommit			= false;
	xfree(opt.partition);
	opt.plane_size			= NO_VAL;
	opt.power_flags			= 0;
	opt.pn_min_memory		= -1;
	opt.req_switch			= -1;
	opt.shared			= NO_VAL16;
	opt.sockets_per_node		= NO_VAL; /* requested sockets */
	opt.pn_min_tmp_disk		= -1;
	opt.threads_per_core		= NO_VAL; /* requested threads */
	opt.threads_per_core_set	= false;
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
	int (*set_func)(slurm_opt_t *, const char *, const char *, bool);
	int eval_pass;
	int exit_on_error;
};

env_vars_t env_vars[] = {
  {"SBATCH_ACCOUNT",       &arg_set_account,		0,	0 },
  {"SBATCH_ARRAY_INX",     &arg_set_array,		0,	0 },
  {"SBATCH_ACCTG_FREQ",    &arg_set_acctg_freq,		0,	0 },
  {"SBATCH_BATCH",         &arg_set_batch,		0,	0 },
  {"SBATCH_BURST_BUFFER",  &arg_set_bb,			0,	0 },
  {"SBATCH_CHECKPOINT",    &arg_set_checkpoint,		0,	1 },
  {"SBATCH_CHECKPOINT_DIR",&arg_set_checkpoint_dir,	0,	0 },
  {"SBATCH_CLUSTERS",      &arg_set_clusters,		0,	0 },
  {"SLURM_CLUSTERS",       &arg_set_clusters,		0,	0 },
  {"SBATCH_CONSTRAINT",    &arg_set_constraint,		0,	0 },
  {"SBATCH_CLUSTER_CONSTRAINT", &arg_set_cluster_constraint, 0,	0 },
  {"SBATCH_CORE_SPEC",     &arg_set_core_spec,		0,	0 },
  {"SBATCH_CPU_FREQ_REQ",  &arg_set_cpu_freq,		0,	0 },
  {"SBATCH_CPUS_PER_GPU",  &arg_set_cpus_per_gpu,	0,	0 },
  {"SBATCH_DEBUG",         &arg_set_verbose,		0,	0 },
  {"SBATCH_DELAY_BOOT",    &arg_set_delay_boot,		0,	0 },
  {"SBATCH_DISTRIBUTION",  &arg_set_distribution,	0,	0 },
  {"SBATCH_EXCLUSIVE",     &arg_set_exclusive,		0,	0 },
  {"SBATCH_EXPORT",        &arg_set_export,		0,	0 },
  {"SBATCH_GET_USER_ENV",  &arg_set_get_user_env,	0,	0 },
  {"SBATCH_GPUS",	   &arg_set_gpus,		0,	0 },
  {"SBATCH_GPU_BIND",	   &arg_set_gpu_bind,		0,	0 },
  {"SBATCH_GPU_FREQ",	   &arg_set_gpu_freq,		0,	0 },
  {"SBATCH_GPUS_PER_NODE", &arg_set_gpus_per_node,	0,	0 },
  {"SBATCH_GPUS_PER_SOCKET", &arg_set_gpus_per_socket,	0,	0 },
  {"SBATCH_GPUS_PER_TASK", &arg_set_gpus_per_task,	0,	0 },
  {"SBATCH_GRES_FLAGS",    &arg_set_gres_flags,		0,	1 },
  {"SBATCH_HINT",          &arg_set_hint,		1,	0 },
  {"SLURM_HINT",           &arg_set_hint, 		1,	0 },
  {"SBATCH_IMMEDIATE",     &arg_set_immediate,		0,	0 },
  {"SBATCH_JOBID",         &arg_set_jobid,		0,	0 },
  {"SBATCH_JOB_NAME",      &arg_set_job_name,		0,	0 },
  {"SBATCH_MEM_BIND",      &arg_set_mem_bind,		0,	1 },
  {"SBATCH_MEM_PER_GPU",   &arg_set_mem_per_gpu,	0,	0 },
  {"SBATCH_NETWORK",       &arg_set_network,		0,	0 },
  {"SBATCH_NO_KILL",	   &arg_set_no_kill,		0,	0 },
  {"SBATCH_NO_REQUEUE",    &arg_set_no_requeue,		0,	0 },
  {"SBATCH_OPEN_MODE",     &arg_set_open_mode,		0,	0 },
  {"SBATCH_OVERCOMMIT",    &arg_set_overcommit,		0,	0 },
  {"SBATCH_PARTITION",     &arg_set_partition,		0,	0 },
  {"SBATCH_POWER",         &arg_set_power,		0,	0 },
  {"SBATCH_PROFILE",       &arg_set_profile,		0,	0 },
  {"SBATCH_QOS",           &arg_set_qos,		0,	0 },
  {"SBATCH_REQ_SWITCH",    &arg_setcomp_req_switch,	0,	0 },
  {"SBATCH_REQUEUE",       &arg_set_requeue,		0,	0 },
  {"SBATCH_RESERVATION",   &arg_set_reservation,	0,	0 },
  {"SBATCH_SIGNAL",        &arg_set_signal,		0,	1 },
  {"SBATCH_SPREAD_JOB",    &arg_set_spread_job,		0,	0 },
  {"SBATCH_THREAD_SPEC",   &arg_set_thread_spec,	0,	0 },
  {"SBATCH_TIMELIMIT",     &arg_set_time,		0,	1 },
  {"SBATCH_USE_MIN_NODES", &arg_set_use_min_nodes,	0,	0 },
  {"SBATCH_WAIT",          &arg_set_wait,		0,	0 },
  {"SBATCH_WAIT_ALL_NODES",&arg_set_wait_all_nodes,	0,	0 },
  {"SBATCH_WAIT4SWITCH",   &arg_setcomp_req_wait4switch,0,	0 },
  {"SBATCH_WCKEY",         &arg_set_wckey,		0,	0 },
  {NULL, NULL, 0, 0 }
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
	}

	/* Process spank env options */
	if (spank_process_env_options())
		exit(error_exit);
}

/*---[ command line option processing ]-----------------------------------*/

static struct option long_options[] = {
	{"account",       required_argument, 0, 'A'},
	{"array",         required_argument, 0, 'a'},
	{"extra-node-info", required_argument, 0, 'B'},
	{"cpus-per-task", required_argument, 0, 'c'},
	{"cluster-constraint",required_argument,0, LONG_OPT_CLUSTER_CONSTRAINT},
	{"constraint",    required_argument, 0, 'C'},
	{"dependency",    required_argument, 0, 'd'},
	{"chdir",         required_argument, 0, 'D'},
	{"workdir",       required_argument, 0, 'D'},
	{"error",         required_argument, 0, 'e'},
	{"nodefile",      required_argument, 0, 'F'},
	{"gpus",          required_argument, 0, 'G'},
	{"help",          no_argument,       0, 'h'},
	{"hold",          no_argument,       0, 'H'},
	{"input",         required_argument, 0, 'i'},
	{"immediate",     no_argument,       0, 'I'},
	{"job-name",      required_argument, 0, 'J'},
	{"kill-on-invalid-dep", required_argument, 0, LONG_OPT_KILL_INV_DEP},
	{"no-kill",       optional_argument, 0, 'k'},
	{"licenses",      required_argument, 0, 'L'},
	{"distribution",  required_argument, 0, 'm'},
	{"cluster",       required_argument, 0, 'M'},
	{"clusters",      required_argument, 0, 'M'},
	{"tasks",         required_argument, 0, 'n'},
	{"ntasks",        required_argument, 0, 'n'},
	{"nodes",         required_argument, 0, 'N'},
	{"output",        required_argument, 0, 'o'},
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
	{"wait",          no_argument,       0, 'W'},
	{"exclude",       required_argument, 0, 'x'},
	{"acctg-freq",    required_argument, 0, LONG_OPT_ACCTG_FREQ},
	{"batch",         required_argument, 0, LONG_OPT_BATCH},
	{"bb",            required_argument, 0, LONG_OPT_BURST_BUFFER_SPEC},
	{"bbf",           required_argument, 0, LONG_OPT_BURST_BUFFER_FILE},
	{"begin",         required_argument, 0, LONG_OPT_BEGIN},
	{"checkpoint",    required_argument, 0, LONG_OPT_CHECKPOINT},
	{"checkpoint-dir",required_argument, 0, LONG_OPT_CHECKPOINT_DIR},
	{"comment",       required_argument, 0, LONG_OPT_COMMENT},
	{"contiguous",    no_argument,       0, LONG_OPT_CONT},
	{"cores-per-socket", required_argument, 0, LONG_OPT_CORESPERSOCKET},
	{"cpu-freq",         required_argument, 0, LONG_OPT_CPU_FREQ},
	{"cpus-per-gpu",  required_argument, 0, LONG_OPT_CPUS_PER_GPU},
	{"deadline",      required_argument, 0, LONG_OPT_DEADLINE},
	{"delay-boot",    required_argument, 0, LONG_OPT_DELAY_BOOT},
	{"exclusive",     optional_argument, 0, LONG_OPT_EXCLUSIVE},
	{"export",        required_argument, 0, LONG_OPT_EXPORT},
	{"export-file",   required_argument, 0, LONG_OPT_EXPORT_FILE},
	{"get-user-env",  optional_argument, 0, LONG_OPT_GET_USER_ENV},
	{"gres",          required_argument, 0, LONG_OPT_GRES},
	{"gres-flags",    required_argument, 0, LONG_OPT_GRES_FLAGS},
	{"gid",           required_argument, 0, LONG_OPT_GID},
	{"gpu-bind",      required_argument, 0, LONG_OPT_GPU_BIND},
	{"gpu-freq",      required_argument, 0, LONG_OPT_GPU_FREQ},
	{"gpus-per-node", required_argument, 0, LONG_OPT_GPUS_PER_NODE},
	{"gpus-per-socket", required_argument, 0, LONG_OPT_GPUS_PER_SOCKET},
	{"gpus-per-task", required_argument, 0, LONG_OPT_GPUS_PER_TASK},
	{"hint",          required_argument, 0, LONG_OPT_HINT},
	{"ignore-pbs",    no_argument,       0, LONG_OPT_IGNORE_PBS},
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
	{"no-requeue",    no_argument,       0, LONG_OPT_NO_REQUEUE},
	{"ntasks-per-core",  required_argument, 0, LONG_OPT_NTASKSPERCORE},
	{"ntasks-per-node",  required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"ntasks-per-socket",required_argument, 0, LONG_OPT_NTASKSPERSOCKET},
	{"open-mode",     required_argument, 0, LONG_OPT_OPEN_MODE},
	{"parsable",      optional_argument, 0, LONG_OPT_PARSABLE},
	{"power",         required_argument, 0, LONG_OPT_POWER},
	{"propagate",     optional_argument, 0, LONG_OPT_PROPAGATE},
	{"profile",       required_argument, 0, LONG_OPT_PROFILE},
	{"priority",      required_argument, 0, LONG_OPT_PRIORITY},
	{"reboot",        no_argument,       0, LONG_OPT_REBOOT},
	{"requeue",       no_argument,       0, LONG_OPT_REQUEUE},
	{"reservation",   required_argument, 0, LONG_OPT_RESERVATION},
	{"signal",        required_argument, 0, LONG_OPT_SIGNAL},
	{"sockets-per-node", required_argument, 0, LONG_OPT_SOCKETSPERNODE},
	{"spread-job",    no_argument,       0, LONG_OPT_SPREAD_JOB},
	{"switches",      required_argument, 0, LONG_OPT_REQ_SWITCH},
	{"tasks-per-node",required_argument, 0, LONG_OPT_NTASKSPERNODE},
	{"test-only",     no_argument,       0, LONG_OPT_TEST_ONLY},
	{"thread-spec",   required_argument, 0, LONG_OPT_THREAD_SPEC},
	{"time-min",      required_argument, 0, LONG_OPT_TIME_MIN},
	{"threads-per-core", required_argument, 0, LONG_OPT_THREADSPERCORE},
	{"tmp",           required_argument, 0, LONG_OPT_TMP},
	{"uid",           required_argument, 0, LONG_OPT_UID},
	{"use-min-nodes", no_argument,       0, LONG_OPT_USE_MIN_NODES},
	{"wait-all-nodes",required_argument, 0, LONG_OPT_WAIT_ALL_NODES},
	{"wckey",         required_argument, 0, LONG_OPT_WCKEY},
	{"wrap",          required_argument, 0, LONG_OPT_WRAP},
#ifdef WITH_SLURM_X11
	{"x11",           optional_argument, 0, LONG_OPT_X11},
#endif
	{NULL,            0,                 0, 0}
};

static char *opt_string =
	"+ba:A:B:c:C:d:D:e:F:G:hHi:IJ:k::L:m:M:n:N:o:Op:P:q:QsS:t:uU:vVw:Wx:";
char *pos_delimit;


/*
 * process_options_first_pass()
 *
 * In this first pass we only look at the command line options, and we
 * will only handle a few options (help, usage, quiet, verbose, version),
 * and look for the script name and arguments (if provided).
 *
 * We will parse the environment variable options, batch script options,
 * and all of the rest of the command line options in
 * process_options_second_pass().
 *
 * Return a pointer to the batch script file name is provided on the command
 * line, otherwise return NULL, and the script will need to be read from
 * standard input.
 */
extern char *process_options_first_pass(int argc, char **argv)
{
	int opt_char, option_index = 0;
	struct option *optz = spank_option_table_create(long_options);
	int i, local_argc = 0;
	char **local_argv, *script_file = NULL;

	if (!optz) {
		error("Unable to create options table");
		exit(error_exit);
	}

	/* initialize option defaults */
	_opt_default(true);

	/* Remove pack job separator and capture all options of interest from
	 * all job components (e.g. "sbatch -N1 -v : -N2 -v tmp" -> "-vv") */
	local_argv = xmalloc(sizeof(char *) * argc);
	for (i = 0; i < argc; i++) {
		if (xstrcmp(argv[i], ":"))
			local_argv[local_argc++] = argv[i];
	}

	opt.progname = xbasename(argv[0]);
	optind = 0;
	while ((opt_char = getopt_long(local_argc, local_argv, opt_string,
				       optz, &option_index)) != -1) {
		switch (opt_char) {
		case '?':
			fprintf(stderr,
				"Try \"sbatch --help\" for more information\n");
			exit(error_exit);
			break;
		case 'h':
			_help();
			exit(0);
			break;
		case 'Q':
			arg_set_quiet(&opt, NULL, "--quiet", false);
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
		case LONG_OPT_WRAP:
			arg_set_wrap(&opt, NULL, "--wrap", false);
			break;
		default:
			/* all others parsed in second pass function */
			break;
		}
	}
	spank_option_table_destroy(optz);

	if ((local_argc > optind) && (sbopt.wrap != NULL)) {
		error("Script arguments not permitted with --wrap option");
		exit(error_exit);
	}
	if (local_argc > optind) {
		int i;
		char **leftover;

		sbopt.script_argc = local_argc - optind;
		leftover = local_argv + optind;
		sbopt.script_argv = xmalloc((sbopt.script_argc + 1)
						 * sizeof(char *));
		for (i = 0; i < sbopt.script_argc; i++)
			sbopt.script_argv[i] = xstrdup(leftover[i]);
		sbopt.script_argv[i] = NULL;
	}
	if (sbopt.script_argc > 0) {
		char *fullpath;
		char *cmd       = sbopt.script_argv[0];
		int  mode       = R_OK;

		if ((fullpath = search_path(opt.cwd, cmd, true, mode, false))) {
			xfree(sbopt.script_argv[0]);
			sbopt.script_argv[0] = fullpath;
		}
		script_file = sbopt.script_argv[0];
	}

	xfree(local_argv);
	return script_file;
}

/* process options:
 * 1. update options with option set in the script
 * 2. update options with env vars
 * 3. update options with commandline args
 * 4. perform some verification that options are reasonable
 *
 * argc IN - Count of elements in argv
 * argv IN - Array of elements to parse
 * argc_off OUT - Offset of first non-parsable element
 * pack_inx IN - pack job component ID, zero origin
 * more_packs OUT - more packs job specifications in script to process
 */
extern void process_options_second_pass(int argc, char **argv, int *argc_off,
					int pack_inx, bool *more_packs,
					const char *file,
					const void *script_body,
					int script_size)
{
	static bool first_pass = true;
	int i;

	/* initialize option defaults */
	_opt_default(first_pass);
	first_pass = false;

	/* set options from batch script */
	*more_packs = _opt_batch_script(file, script_body, script_size,
				        pack_inx);

	for (i = WRPR_START + 1; i < WRPR_CNT; i++) {
		/* Convert command from batch script to sbatch command */
		bool stop = _opt_wrpr_batch_script(file, script_body,
						   script_size, argc, argv, i);

		if (stop)
			break;
	}

	/* set options from env vars */
	_opt_env(0);

	/* set options from command line */
	_set_options(argc, argv);
	*argc_off = optind;

	/* set options from env vars, pass 2 */
	_opt_env(1);

	if (!_opt_verify())
		exit(error_exit);

	if (opt.verbose)
		_opt_list();
}

/*
 * _next_line - Interpret the contents of a byte buffer as characters in
 *	a file.  _next_line will find and return the next line in the buffer.
 *
 *	If "state" is NULL, it will start at the beginning of the buffer.
 *	_next_line will update the "state" pointer to point at the
 *	spot in the buffer where it left off.
 *
 * IN buf - buffer containing file contents
 * IN size - size of buffer "buf"
 * IN/OUT state - used by _next_line to determine where the last line ended
 *
 * RET - xmalloc'ed character string, or NULL if no lines remaining in buf.
 */
static char *_next_line(const void *buf, int size, void **state)
{
	char *line;
	char *current, *ptr;

	if (*state == NULL) /* initial state */
		*state = (void *)buf;

	if ((*state - buf) >= size) /* final state */
		return NULL;

	ptr = current = (char *)*state;
	while ((*ptr != '\n') && (ptr < ((char *)buf+size)))
		ptr++;

	line = xstrndup(current, ptr-current);

	/*
	 *  Advance state past newline
	 */
	*state = (ptr < ((char *) buf + size)) ? ptr+1 : ptr;
	return line;
}

/*
 * _get_argument - scans a line for something that looks like a command line
 *	argument, and return an xmalloc'ed string containing the argument.
 *	Quotes can be used to group characters, including whitespace.
 *	Quotes can be included in an argument be escaping the quotes,
 *	preceding the quote with a backslash (\").
 *
 * IN - line
 * OUT - skipped - number of characters parsed from line
 * RET - xmalloc'ed argument string (may be shorter than "skipped")
 *       or NULL if no arguments remaining
 */
static char *
_get_argument(const char *file, int lineno, const char *line, int *skipped)
{
	const char *ptr;
	char *argument = NULL;
	char q_char = '\0';
	bool escape_flag = false;
	bool quoted = false;
	int i;

	ptr = line;
	*skipped = 0;

	if ((argument = strcasestr(line, "packjob")))
		memcpy(argument, "       ", 7);

	/* skip whitespace */
	while (isspace(*ptr) && *ptr != '\0') {
		ptr++;
	}

	if (*ptr == ':') {
		fatal("%s: line %d: Unexpected `:` in [%s]",
		      file, lineno, line);
	}

	if (*ptr == '\0')
		return NULL;

	/* copy argument into "argument" buffer, */
	i = 0;
	while ((quoted || !isspace(*ptr)) && (*ptr != '\n') && (*ptr != '\0')) {
		if (escape_flag) {
			escape_flag = false;
		} else if (*ptr == '\\') {
			escape_flag = true;
			ptr++;
			continue;
		} else if (quoted) {
			if (*ptr == q_char) {
				quoted = false;
				ptr++;
				continue;
			}
		} else if ((*ptr == '"') || (*ptr == '\'')) {
			quoted = true;
			q_char = *(ptr++);
			continue;
		} else if (*ptr == '#') {
			/* found an un-escaped #, rest of line is a comment */
			break;
		}

		if (!argument)
			argument = xmalloc(strlen(line) + 1);
		argument[i++] = *(ptr++);
	}

	if (quoted) {	/* Unmatched quote */
		fatal("%s: line %d: Unmatched `%c` in [%s]",
		      file, lineno, q_char, line);
	}

	*skipped = ptr - line;

	return argument;
}

/*
 * set options from batch script
 *
 * Build an argv-style array of options from the script "body",
 * then pass the array to _set_options for() further parsing.
 * RET - True if more pack job specifications to process
 */
static bool _opt_batch_script(const char * file, const void *body, int size,
			      int pack_inx)
{
	char *magic_word1 = "#SBATCH";
	char *magic_word2 = "#SLURM";
	int magic_word_len1, magic_word_len2;
	int argc;
	char **argv;
	void *state = NULL;
	char *line;
	char *option;
	char *ptr;
	int skipped = 0, warned = 0, lineno = 0;
	int i, pack_scan_inx = 0;
	bool more_packs = false;

	magic_word_len1 = strlen(magic_word1);
	magic_word_len2 = strlen(magic_word2);

	/* getopt_long skips over the first argument, so fill it in */
	argc = 1;
	argv = xmalloc(sizeof(char *));
	argv[0] = "sbatch";

	while ((line = _next_line(body, size, &state)) != NULL) {
		lineno++;
		if (!xstrncmp(line, magic_word1, magic_word_len1))
			ptr = line + magic_word_len1;
		else if (!xstrncmp(line, magic_word2, magic_word_len2)) {
			ptr = line + magic_word_len2;
			if (!warned) {
				error("Change from #SLURM to #SBATCH in your "
				      "script and verify the options are "
				      "valid in sbatch");
				warned = 1;
			}
		} else {
			/* Stop parsing script if not a comment */
			bool is_cmd = false;
			for (i = 0; line[i]; i++) {
				if (isspace(line[i]))
					continue;
				if (line[i] == '#')
					break;
				is_cmd = true;
				break;
			}
			xfree(line);
			if (is_cmd)
				break;
			continue;
		}

		/* this line starts with the magic word */
		if (strcasestr(line, "packjob"))
			pack_scan_inx++;
		if (pack_scan_inx < pack_inx) {
			xfree(line);
			continue;
		}
		if (pack_scan_inx > pack_inx) {
			more_packs = true;
			xfree(line);
			break;
		}

		while ((option = _get_argument(file, lineno, ptr, &skipped))) {
			debug2("Found in script, argument \"%s\"", option);
			argc++;
			xrealloc(argv, sizeof(char *) * argc);
			argv[argc - 1] = option;
			ptr += skipped;
		}
		xfree(line);
	}

	if (argc > 0)
		_set_options(argc, argv);

	for (i = 1; i < argc; i++)
		xfree(argv[i]);
	xfree(argv);

	return more_packs;
}

/*
 * set wrapper (ie. pbs, bsub) options from batch script
 *
 * Build an argv-style array of options from the script "body",
 * then pass the array to _set_options for() further parsing.
 */
static bool _opt_wrpr_batch_script(const char *file, const void *body,
				   int size, int cmd_argc, char **cmd_argv,
				   int magic)
{
	char *magic_word;
	void (*wrp_func) (int,char**) = NULL;
	int magic_word_len;
	int argc;
	char **argv;
	void *state = NULL;
	char *line;
	char *option;
	char *ptr;
	int skipped = 0;
	int lineno = 0;
	int non_comments = 0;
	int i;
	bool found = false;

	if (sbopt.ignore_pbs)
		return false;
	if (getenv("SBATCH_IGNORE_PBS"))
		return false;
	for (i = 0; i < cmd_argc; i++) {
		if (!xstrcmp(cmd_argv[i], "--ignore-pbs"))
			return false;
	}

	/* Check what command it is */
	switch (magic) {
	case WRPR_BSUB:
		magic_word = "#BSUB";
		wrp_func = _set_bsub_options;
		break;
	case WRPR_PBS:
		magic_word = "#PBS";
		wrp_func = _set_pbs_options;
		break;

	default:
		return false;
	}

	magic_word_len = strlen(magic_word);
	/* getopt_long skips over the first argument, so fill it in */
	argc = 1;
	argv = xmalloc(sizeof(char *));
	argv[0] = "sbatch";

	while ((line = _next_line(body, size, &state)) != NULL) {
		lineno++;
		if (xstrncmp(line, magic_word, magic_word_len) != 0) {
			if (line[0] != '#')
				non_comments++;
			xfree(line);
			if (non_comments > 100)
				break;
			continue;
		}

		/* Set found to be true since we found a valid command */
		found = true;
		/* this line starts with the magic word */
		ptr = line + magic_word_len;
		while ((option = _get_argument(file, lineno, ptr, &skipped))) {
			debug2("Found in script, argument \"%s\"", option);
			argc += 1;
			xrealloc(argv, sizeof(char*) * argc);

			/* Only check the even options here (they are
			 * the - options) */
			if (magic == WRPR_BSUB && !(argc%2)) {
				/* Since Slurm doesn't allow long
				 * names with a single '-' we must
				 * translate before hand.
				 */
				if (!xstrcmp("-cwd", option)) {
					xfree(option);
					option = xstrdup("-c");
				}
			}

			argv[argc-1] = option;
			ptr += skipped;
		}
		xfree(line);
	}

	if ((argc > 0) && (wrp_func != NULL))
		wrp_func(argc, argv);

	for (i = 1; i < argc; i++)
		xfree(argv[i]);
	xfree(argv);

	return found;
}

static void _set_options(int argc, char **argv)
{
	int opt_char, option_index = 0;
	struct option *optz = spank_option_table_create(long_options);

	if (!optz) {
		error("Unable to create options table");
		exit(error_exit);
	}

	optind = 0;
	while ((opt_char = getopt_long(argc, argv, opt_string,
				       optz, &option_index)) != -1) {
		switch (opt_char) {
		case '?':
			/* handled in process_options_first_pass() */
			break;
		case 'a':
			arg_set_array(&opt, optarg, "--array", false);
			break;
		case 'A':
		case 'U':	/* backwards compatibility */
			arg_set_account(&opt, optarg, "--account", false);
			break;
		case 'B':
			arg_set_extra_node_info(&opt, optarg, "--extra-node-info", true);
			break;
		case 'c':
			arg_set_cpus_per_task(&opt, optarg, "cpus-per-task", true);
			break;
		case 'C':
			arg_set_constraint(&opt, optarg, "--constraint", false);
			break;
		case 'd':
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case 'D':
			arg_set_workdir(&opt, optarg, "--workdir", false);
			break;
		case 'e':
			arg_set_error(&opt, optarg, "--error", true);
			break;
		case 'F':
			arg_set_nodefile(&opt, optarg, "--nodefile", true);
			break;
		case 'G':
			arg_set_gpus(&opt, optarg, "--gpus", false);
			break;
		case 'h':
			/* handled in process_options_first_pass() */
			break;
		case 'H':
			arg_set_hold(&opt, NULL, "--hold", false);
			break;
		case 'i':
			arg_set_input(&opt, optarg, "--input", false);
			break;
		case 'I':
			arg_set_immediate(&opt, NULL, "--immediate", false);
			break;
		case 'J':
			arg_set_job_name(&opt, optarg, "--job-name", false);
			break;
		case 'k':
			arg_set_no_kill(&opt, optarg, "--no-kill", false);
			break;
		case 'L':
			arg_set_licenses(&opt, optarg, "--licenses", false);
			break;
		case 'm':
			arg_set_distribution(&opt, optarg, "--distribution",
					     true);
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
		case 'o':
			arg_set_output(&opt, optarg, "--output", false);
			break;
		case 'O':
			arg_set_overcommit(&opt, NULL, "--overcommit", false);
			break;
		case 'p':
			arg_set_partition(&opt, optarg, "--partition", false);
			break;
		case 'P':
			verbose("-P option is deprecated, use -d instead");
			arg_set_dependency(&opt, optarg, "--dependency", false);
			break;
		case 'q':
			arg_set_qos(&opt, optarg, "--qos", false);
			break;
		case 'Q':
			/* handled in process_options_first_pass() */
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
		case 'v':
		case 'V':
			/* handled in process_options_first_pass() */
			break;
		case 'w':
			arg_set_nodelist(&opt, optarg, "--nodelist", false);
			break;
		case 'W':
			arg_set_wait(&opt, NULL, "--wait", false);
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
			arg_set_mem_per_gpu(&opt, optarg, "--mem-per-gpu", false);
			break;
		case LONG_OPT_MEM_BIND:
			arg_set_mem_bind(&opt, optarg, "--mem-bind", true);
			break;
		case LONG_OPT_MINCPU:
			arg_set_mincpus(&opt, optarg, "--mincpus", true);
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
		case LONG_OPT_JOBID:
			arg_set_jobid(&opt, optarg, "--jobid", false);
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
		case LONG_OPT_BURST_BUFFER_SPEC:
			arg_set_bb(&opt, optarg, "--bb", false);
			break;
		case LONG_OPT_BURST_BUFFER_FILE:
			arg_set_bbf(&opt, optarg, "--bbf", false);
			break;
		case LONG_OPT_NICE:
			arg_set_nice(&opt, optarg, "--nice", false);
			break;
		case LONG_OPT_PRIORITY:
			arg_set_priority(&opt, optarg, "--priority", true);
			break;
		case LONG_OPT_NO_REQUEUE:
			arg_set_no_requeue(&opt, NULL, "--no-requeue", false);
			break;
		case LONG_OPT_REQUEUE:
			arg_set_requeue(&opt, NULL, "--requeue", false);
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
		case LONG_OPT_BATCH:
			arg_set_batch(&opt, optarg, "--batch", false);
			break;
		case LONG_OPT_REBOOT:
			arg_set_reboot(&opt, NULL, "--reboot", false);
			break;
		case LONG_OPT_WRAP:
			/* handled in process_options_first_pass() */
			break;
		case LONG_OPT_GET_USER_ENV:
			arg_set_get_user_env(&opt, optarg, "--get-user-env", false);
			break;
		case LONG_OPT_OPEN_MODE:
			arg_set_open_mode(&opt, optarg, "--open-mode", false);
			break;
		case LONG_OPT_ACCTG_FREQ:
			arg_set_acctg_freq(&opt, optarg, "--acctg-freq", true);
			break;
		case LONG_OPT_PROPAGATE:
			arg_set_propagate(&opt, optarg, "--propagate", false);
			break;
		case LONG_OPT_NETWORK:
			arg_set_network(&opt, optarg, "--network", false);
			break;
		case LONG_OPT_WCKEY:
			arg_set_wckey(&opt, optarg, "--wckey", false);
			break;
		case LONG_OPT_RESERVATION:
			arg_set_reservation(&opt, optarg, "--reservation", false);
			break;
		case LONG_OPT_CHECKPOINT:
			arg_set_checkpoint(&opt, optarg, "--checkpoint", true);
			break;
		case LONG_OPT_CHECKPOINT_DIR:
			arg_set_checkpoint_dir(&opt, optarg, "--checkpoint-dir", false);
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
		case LONG_OPT_EXPORT:
			arg_set_export(&opt, optarg, "--export", false);
			break;
		case LONG_OPT_EXPORT_FILE:
			arg_set_export_file(&opt, optarg, "--export-file", false);
			break;
		case LONG_OPT_CPU_FREQ:
			arg_set_cpu_freq(&opt, optarg, "--cpu-freq", false);
			break;
		case LONG_OPT_REQ_SWITCH:
			arg_set_switches(&opt, optarg, "--switches", false);
			break;
		case LONG_OPT_IGNORE_PBS:
			arg_set_ignore_pbs(&opt, NULL, "--ignore-pbs", false);
			break;
		case LONG_OPT_TEST_ONLY:
			arg_set_test_only(&opt, NULL, "--test-only", false);
			break;
		case LONG_OPT_PARSABLE:
			arg_set_parsable(&opt, optarg, "--parsable", false);
			break;
		case LONG_OPT_POWER:
			arg_set_power(&opt, optarg, "--power", false);
			break;
		case LONG_OPT_THREAD_SPEC:
			arg_set_thread_spec(&opt, optarg, "--thread-spec", false);
			break;
		case LONG_OPT_KILL_INV_DEP:
			arg_set_kill_on_invalid_dep(&opt, optarg, "--kill-on-invalid-dep", false);
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
			if (spank_process_option (opt_char, optarg) < 0)
				exit(error_exit);
		}
	}

	spank_option_table_destroy (optz);
}

static void _set_bsub_options(int argc, char **argv) {

	int opt_char, option_index = 0;
	char *bsub_opt_string = "+c:e:J:m:M:n:o:q:W:x";
	char *tmp_str, *char_ptr;

	struct option bsub_long_options[] = {
		{"cwd", required_argument, 0, 'c'},
		{"error_file", required_argument, 0, 'e'},
		{"job_name", required_argument, 0, 'J'},
		{"hostname", required_argument, 0, 'm'},
		{"memory_limit", required_argument, 0, 'M'},
		{"num_processors", required_argument, 0, 'n'},
		{"output_file", required_argument, 0, 'o'},
		{"queue_name", required_argument, 0, 'q'},
		{"time", required_argument, 0, 'W'},
		{"exclusive", no_argument, 0, 'x'},
		{NULL, 0, 0, 0}
	};

	optind = 0;
	while ((opt_char = getopt_long(argc, argv, bsub_opt_string,
				       bsub_long_options, &option_index))
	       != -1) {
		switch (opt_char) {
		case 'c':
			arg_set_workdir(&opt, optarg, "cwd", false);
			break;
		case 'e':
			arg_set_error(&opt, optarg, "error_file", false);
			break;
		case 'J':
			arg_set_job_name(&opt, optarg, "job_name", false);
			break;
		case 'm':
			/* Since BSUB requires a list of space
			   sperated host we need to replace the spaces
			   with , */
			tmp_str = xstrdup(optarg);
			char_ptr = strstr(tmp_str, " ");

			while (char_ptr != NULL) {
				*char_ptr = ',';
				char_ptr = strstr(tmp_str, " ");
			}
			arg_set_nodelist(&opt, optarg, "hostname", false);
			break;
		case 'M':
			arg_set_mem_per_cpu(&opt, optarg, "memory_limit", false);
			break;
		case 'n':
			/* Since it is value in bsub to give a min and
			 * max task count we will only read the max if
			 * it exists.
			 */
			char_ptr = strstr(optarg, ",");
			if (char_ptr) {
				char_ptr++;
				if (!char_ptr[0]) {
					error("#BSUB -n format not correct "
					      "given: '%s'",
					      optarg);
					exit(error_exit);
				}
			} else
				char_ptr = optarg;

			arg_set_ntasks(&opt, char_ptr, "num_processors", false);
			break;
		case 'o':
			arg_set_output(&opt, optarg, "output_file", false);
			break;
		case 'q':
			arg_set_partition(&opt, optarg, "queue_name", false);
			break;
		case 'W':
			arg_set_time(&opt, optarg, "time", true);
			break;
		case 'x':
			arg_set_exclusive(&opt, NULL, "exclusive", false);
			break;
		default:
			error("Unrecognized command line parameter %c",
			      opt_char);
			exit(error_exit);
		}
	}


	if (optind < argc) {
		error("Invalid argument: %s", argv[optind]);
		exit(error_exit);
	}

}

static void _set_pbs_options(int argc, char **argv)
{
	int opt_char, option_index = 0;
	char *pbs_opt_string = "+a:A:c:C:e:hIj:J:k:l:m:M:N:o:p:q:r:S:t:u:v:VW:z";

	struct option pbs_long_options[] = {
		{"start_time", required_argument, 0, 'a'},
		{"account", required_argument, 0, 'A'},
		{"checkpoint", required_argument, 0, 'c'},
		{"working_dir", required_argument, 0, 'C'},
		{"error", required_argument, 0, 'e'},
		{"hold", no_argument, 0, 'h'},
		{"interactive", no_argument, 0, 'I'},
		{"join", optional_argument, 0, 'j'},
		{"job_array", required_argument, 0, 'J'},
		{"keep", required_argument, 0, 'k'},
		{"resource_list", required_argument, 0, 'l'},
		{"mail_options", required_argument, 0, 'm'},
		{"mail_user_list", required_argument, 0, 'M'},
		{"job_name", required_argument, 0, 'N'},
		{"out", required_argument, 0, 'o'},
		{"priority", required_argument, 0, 'p'},
		{"destination", required_argument, 0, 'q'},
		{"rerunable", required_argument, 0, 'r'},
		{"script_path", required_argument, 0, 'S'},
		{"array", required_argument, 0, 't'},
		{"running_user", required_argument, 0, 'u'},
		{"variable_list", required_argument, 0, 'v'},
		{"all_env", no_argument, 0, 'V'},
		{"attributes", required_argument, 0, 'W'},
		{"no_std", no_argument, 0, 'z'},
		{NULL, 0, 0, 0}
	};

	optind = 0;
	while ((opt_char = getopt_long(argc, argv, pbs_opt_string,
				       pbs_long_options, &option_index))
	       != -1) {
		switch (opt_char) {
		case 'a':
			arg_set_begin(&opt, optarg, "start_time", false);
			break;
		case 'A':
			arg_set_account(&opt, optarg, "account", false);
			break;
		case 'c':
			/* DMJ: is there some reason checkpoint not implemented */
			break;
		case 'C':
			/* DMJ: is there some reason working_dir not implemented */
			break;
		case 'e':
			arg_set_error(&opt, optarg, "error", false);
			break;
		case 'h':
			arg_set_hold(&opt, NULL, "hold", false);
			break;
		case 'I':
			break;
		case 'j':
			break;
		case 'J':
		case 't':
			/* PBS Pro uses -J. Torque uses -t. */
			arg_set_array(&opt, optarg, "job_array", false);
			break;
		case 'k':
			break;
		case 'l':
			_parse_pbs_resource_list(optarg);
			break;
		case 'm':
			if (!optarg) /* CLANG Fix */
				break;
			arg_set_pbsmail_type(&opt, optarg, "mail_options", true);
			break;
		case 'M':
			arg_set_mail_user(&opt, optarg, "mail_user_list", false);
			break;
		case 'N':
			arg_set_job_name(&opt, optarg, "job_name", false);
			break;
		case 'o':
			arg_set_output(&opt, optarg, "out", false);
			break;
		case 'p': {
			arg_set_nice(&opt, optarg, "priority", false);
			break;
		}
		case 'q':
			arg_set_partition(&opt, optarg, "destination", false);
			break;
		case 'r':
			break;
		case 'S':
			break;
		case 'u':
			break;
		case 'v': {
			const char *curr = arg_get_export(&opt);
			char *temp = NULL;
			char *sep = "";
			if (curr)
				sep = ",";
			temp = xstrdup_printf("%s%s%s", curr ? curr : "", sep,
					      optarg);
			xstrfmtcat(sbopt.export_env, "%s%s", sep, optarg);
			arg_set_export(&opt, temp, "variable_list", false);
			xfree(temp);
			xfree(curr);
			break;
		}
		case 'V':
			break;
		case 'W':
			if (!optarg) /* CLANG Fix */
				break;
			if (!xstrncasecmp(optarg, "umask=", 6)) {
				arg_set_umask(&opt, optarg+6, "umask attribute", false);
			} else if (!xstrncasecmp(optarg, "depend=", 7)) {
				arg_set_dependency(&opt, optarg+7, "depend attribute", false);
			} else {
				verbose("Ignored PBS attributes: %s", optarg);
			}
			break;
		case 'z':
			break;
		default:
			error("Unrecognized command line parameter %c",
			      opt_char);
			exit(error_exit);
		}
	}

	if (optind < argc) {
		error("Invalid argument: %s", argv[optind]);
		exit(error_exit);
	}
}

static char *_get_pbs_node_name(char *node_options, int *i)
{
	int start = (*i);
	char *value = NULL;

	while (node_options[*i] &&
	       (node_options[*i] != '+') &&
	       (node_options[*i] != ':'))
		(*i)++;

	value = xmalloc((*i)-start+1);
	memcpy(value, node_options+start, (*i)-start);

	if (node_options[*i])
		(*i)++;

	return value;
}

static void _get_next_pbs_node_part(char *node_options, int *i)
{
	while (node_options[*i] &&
	       (node_options[*i] != '+') &&
	       (node_options[*i] != ':'))
		(*i)++;
	if (node_options[*i])
		(*i)++;
}

static void _parse_pbs_nodes_opts(char *node_opts)
{
	int i = 0;
	char *temp = NULL;
	int ppn = 0;
	int node_cnt = 0;
	hostlist_t hl = hostlist_create(NULL);

	while (node_opts[i]) {
		if (!xstrncmp(node_opts+i, "ppn=", 4)) {
			i+=4;
			ppn += strtol(node_opts+i, NULL, 10);
			_get_next_pbs_node_part(node_opts, &i);
		} else if (isdigit(node_opts[i])) {
			node_cnt += strtol(node_opts+i, NULL, 10);
			_get_next_pbs_node_part(node_opts, &i);
		} else if (isalpha(node_opts[i])) {
			temp = _get_pbs_node_name(node_opts, &i);
			hostlist_push_host(hl, temp);
			xfree(temp);
		} else
			i++;

	}

	/* DMJ WARNING: this looks like a bug; node_cnt will never get set in
	 * the opt structure */
	if (!node_cnt)
		node_cnt = 1;
	else {
		char *temp = xstrdup_printf("%d", node_cnt);
		arg_set_nodes(&opt, temp, "nodes", false);
		xfree(temp);
	}

	if (ppn) {
		char *temp = NULL;
		ppn *= node_cnt;
		temp = xstrdup_printf("%d", ppn);
		arg_set_ntasks(&opt, temp, "nodes ntasks", false);
		xfree(temp);
	}

	if (hostlist_count(hl) > 0) {
		char *temp = hostlist_ranged_string_xmalloc(hl);
		arg_set_nodelist(&opt, temp, "nodes nodelist", false);
		xfree(temp);
	}

	hostlist_destroy(hl);
}

static void _get_next_pbs_option(char *pbs_options, int *i)
{
	while (pbs_options[*i] && pbs_options[*i] != ',')
		(*i)++;
	if (pbs_options[*i])
		(*i)++;
}

static char *_get_pbs_option_value(char *pbs_options, int *i, char sep)
{
	int start = (*i);
	char *value = NULL;

	while (pbs_options[*i] && pbs_options[*i] != sep)
		(*i)++;
	value = xmalloc((*i)-start+1);
	memcpy(value, pbs_options+start, (*i)-start);

	if (pbs_options[*i])
		(*i)++;

	return value;
}

static void _parse_pbs_resource_list(char *rl)
{
	int i = 0;
	int gpus = 0;
	char *temp = NULL;
	int pbs_pro_flag = 0;	/* Bits: select:1 ncpus:2 mpiprocs:4 */

	while (rl[i]) {
		if (!xstrncasecmp(rl+i, "accelerator=", 12)) {
			i += 12;
			if (!xstrncasecmp(rl+i, "true", 4) && (gpus < 1))
				gpus = 1;
			/* Also see "naccelerators=" below */
		} else if (!xstrncmp(rl+i, "arch=", 5)) {
			i+=5;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "cput=", 5)) {
			i+=5;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for cput");
				exit(error_exit);
			}
			arg_set_time(&opt, temp, "cput", true);
		} else if (!xstrncmp(rl+i, "file=", 5)) {
			int end = 0;
			long mbytes = 0;

			i+=5;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for file");
				exit(error_exit);
			}
			end = strlen(temp) - 1;
			if (toupper(temp[end]) == 'B') {
				/* In Torque they do GB or MB on the
				 * end of size, we just want G or M so
				 * we will remove the b on the end
				 */
				temp[end] = '\0';
			}
			mbytes = str_to_mbytes(temp);
			xfree(temp);
			arg_set_tmp_mb(&opt, mbytes, "file", true);
		} else if (!xstrncmp(rl+i, "host=", 5)) {
			i+=5;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "mem=", 4)) {
			int end = 0;
			long mbytes = 0;

			i+=4;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for mem");
				exit(error_exit);
			}
			end = strlen(temp) - 1;
			if (toupper(temp[end]) == 'B') {
				/* In Torque they do GB or MB on the
				 * end of size, we just want G or M so
				 * we will remove the b on the end
				 */
				temp[end] = '\0';
			}
			mbytes = str_to_mbytes(temp);
			xfree(temp);
			arg_set_mem_mb(&opt, (int64_t) mbytes, "mem", true);
		} else if (!xstrncasecmp(rl+i, "mpiprocs=", 9)) {
			i += 9;
			temp = _get_pbs_option_value(rl, &i, ':');
			if (temp) {
				pbs_pro_flag |= 4;
				arg_set_ntasks_per_node(&opt, temp, "mpiprocs", false);
			}
			xfree(temp);
#ifdef HAVE_NATIVE_CRAY
			/*
			 * NB: no "mppmem" here since it specifies per-PE memory units,
			 *     whereas Slurm uses per-node and per-CPU memory units.
			 */
		} else if (!xstrncmp(rl + i, "mppdepth=", 9)) {
			/* Cray: number of CPUs (threads) per processing element */
			i += 9;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (temp)
				arg_set_cpus_per_task(&opt, temp, "mppdepth", true);
			xfree(temp);
		} else if (!xstrncmp(rl + i, "mppnodes=", 9)) {
			/* Cray `nodes' variant: hostlist without prefix */
			i += 9;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for mppnodes");
				exit(error_exit);
			}
			arg_set_nodelist(&opt, temp, "mppnodes", false);
			xfree(temp);
		} else if (!xstrncmp(rl + i, "mppnppn=", 8)) {
			/* Cray: number of processing elements per node */
			i += 8;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (temp)
				arg_set_ntasks_per_node(&opt, temp, "mppnppn", false);
			xfree(temp);
		} else if (!xstrncmp(rl + i, "mppwidth=", 9)) {
			/* Cray: task width (number of processing elements) */
			i += 9;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (temp)
				arg_set_ntasks(&opt, temp, "mppwidth", false);
			xfree(temp);
#endif /* HAVE_NATIVE_CRAY */
		} else if (!xstrncasecmp(rl+i, "naccelerators=", 14)) {
			i += 14;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (temp) {
				gpus = parse_int("naccelerators", temp, false);
				xfree(temp);
			}
		} else if (!xstrncasecmp(rl+i, "ncpus=", 6)) {
			i += 6;
			temp = _get_pbs_option_value(rl, &i, ':');
			if (temp) {
				pbs_pro_flag |= 2;
				arg_set_mincpus(&opt, temp, "ncpus", false);
			}
			xfree(temp);
		} else if (!xstrncmp(rl+i, "nice=", 5)) {
			i += 5;
			temp = _get_pbs_option_value(rl, &i, ',');
			arg_set_nice(&opt, temp, "nice", false);
			xfree(temp);
		} else if (!xstrncmp(rl+i, "nodes=", 6)) {
			i+=6;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for nodes");
				exit(error_exit);
			}
			_parse_pbs_nodes_opts(temp);
			xfree(temp);
		} else if (!xstrncmp(rl+i, "opsys=", 6)) {
			i+=6;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "other=", 6)) {
			i+=6;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "pcput=", 6)) {
			i+=6;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for pcput");
				exit(error_exit);
			}
			arg_set_time(&opt, temp, "pcput", true);
			xfree(temp);
		} else if (!xstrncmp(rl+i, "pmem=", 5)) {
			i+=5;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "proc=", 5)) {
			i += 5;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (temp) {
				const char *curr = arg_get_constraint(&opt);
				char *tmp = xstrdup_printf("%s,%s", curr, temp);
				arg_set_constraint(&opt, tmp, "proc", false);
				xfree(tmp);
				xfree(curr);
			}
			xfree(temp);
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "pvmem=", 6)) {
			i+=6;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncasecmp(rl+i, "select=", 7)) {
			i += 7;
			temp = _get_pbs_option_value(rl, &i, ':');
			if (temp) {
				pbs_pro_flag |= 1;
				arg_set_nodes(&opt, temp, "select", true);
			}
			xfree(temp);
		} else if (!xstrncmp(rl+i, "software=", 9)) {
			i+=9;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "vmem=", 5)) {
			i+=5;
			_get_next_pbs_option(rl, &i);
		} else if (!xstrncmp(rl+i, "walltime=", 9)) {
			i+=9;
			temp = _get_pbs_option_value(rl, &i, ',');
			if (!temp) {
				error("No value given for walltime");
				exit(error_exit);
			}
			arg_set_time(&opt, temp, "walltime", true);
			xfree(temp);
		} else
			i++;
	}

	if ((pbs_pro_flag == 7) && (opt.pn_min_cpus > opt.ntasks_per_node)) {
		/* This logic will allocate the proper CPU count on each
		 * node if the CPU count per node is evenly divisible by
		 * the task count on each node. Slurm can't handle something
		 * like cpus_per_node=10 and ntasks_per_node=8 */
		arg_set_cpus_per_task_int(&opt, 
					  opt.pn_min_cpus / opt.ntasks_per_node,
					  "ntasks", true);
	}
	if (gpus > 0) {
		char *temp = NULL;
		const char *curr = arg_get_gres(&opt);
		char *sep = "";
		if (curr)
			sep = ",";
		temp = xstrdup_printf("%s%sgpu:%d", curr ? curr : "",
				      sep, gpus);
		arg_set_gres(&opt, temp, "gpu gres", false);
		xfree(temp);
		xfree(curr);
	}
}

/*
 * _opt_verify : perform some post option processing verification
 *
 */
static bool _opt_verify(void)
{
	bool verified = true;
	char *dist = NULL, *dist_lllp = NULL;
	hostlist_t hl = NULL;
	int hl_cnt = 0;

	if (opt.quiet && opt.verbose) {
		error ("don't specify both --verbose (-v) and --quiet (-Q)");
		verified = false;
	}

	/* DMJ needs work */
	if (opt.hint_env &&
	    (!opt.hint_set && !opt.ntasks_per_core_set &&
	     !opt.threads_per_core_set)) {
		if (verify_hint(opt.hint_env,
				&opt.sockets_per_node,
				&opt.cores_per_socket,
				&opt.threads_per_core,
				&opt.ntasks_per_core,
				NULL)) {
			exit(error_exit);
		}
	}

	_fullpath(&sbopt.efname, opt.cwd);
	_fullpath(&sbopt.ifname, opt.cwd);
	_fullpath(&sbopt.ofname, opt.cwd);

	/* DMJ needs work */
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
				error("Failure getting NodeNames from hostfile");
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

	/* DMJ needs work */
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

	if ((opt.ntasks_per_node > 0) && (!opt.ntasks_set) &&
	    ((opt.max_nodes == 0) || (opt.min_nodes == opt.max_nodes))) {
		int ntasks = opt.min_nodes * opt.ntasks_per_node;
		arg_set_ntasks_int(&opt, ntasks, "ntasks", false);
	}

	if (opt.cpus_set && (opt.pn_min_cpus < opt.cpus_per_task))
		arg_set_mincpus_int(&opt, opt.cpus_per_task, "<internal>", false);

	if ((opt.job_name == NULL) && (sbopt.script_argc > 0)) {
		char *temp = base_name(sbopt.script_argv[0]);
		arg_set_job_name(&opt, temp, "job-name", false);
	}
	if (opt.job_name)
		setenv("SLURM_JOB_NAME", opt.job_name, 1);

	/* check for realistic arguments */
	if (opt.ntasks < 0) {
		error("invalid number of tasks (-n %d)", opt.ntasks);
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
		if ((opt.min_nodes <= 0) ||
		    ((opt.ntasks/opt.plane_size) < opt.min_nodes)) {
			if (((opt.min_nodes-1)*opt.plane_size) >= opt.ntasks) {
#if (0)
				info("Too few processes ((n/plane_size) %d < N %d) "
				     "and ((N-1)*(plane_size) %d >= n %d)) ",
				     opt.ntasks/opt.plane_size, opt.min_nodes,
				     (opt.min_nodes-1)*opt.plane_size, opt.ntasks);
#endif
				error("Too few processes for the requested "
				      "{plane,node} distribution");
				exit(error_exit);
			}
		}
	}

	set_distribution(opt.distribution, &dist, &dist_lllp);
	if (dist)
		 pack_env.dist = xstrdup(dist);
	if ((opt.distribution & SLURM_DIST_STATE_BASE) == SLURM_DIST_PLANE)
		 pack_env.plane_size = opt.plane_size;
	if (dist_lllp)
		 pack_env.dist_lllp = xstrdup(dist_lllp);

	/* massage the numbers */
	if ((opt.nodes_set || opt.extra_set)				&&
	    ((opt.min_nodes == opt.max_nodes) || (opt.max_nodes == 0))	&&
	    !opt.ntasks_set) {
		/* 1 proc / node default */
		opt.ntasks = MAX(opt.min_nodes, 1);

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

	if (opt.ntasks_set && (opt.ntasks > 0))
		pack_env.ntasks = opt.ntasks;

	if (hl)
		hostlist_destroy(hl);

	if ((opt.deadline) && (opt.begin) && (opt.deadline < opt.begin)) {
		error("Incompatible begin and deadline time specification");
		exit(error_exit);
	}

	if ((opt.euid != (uid_t) -1) && (opt.euid != opt.uid))
		opt.uid = opt.euid;

	if ((opt.egid != (gid_t) -1) && (opt.egid != opt.gid))
		opt.gid = opt.egid;

	if (opt.dependency)
		setenvfs("SLURM_JOB_DEPENDENCY=%s", opt.dependency);

	if (opt.profile)
		setenvfs("SLURM_PROFILE=%s",
			 acct_gather_profile_to_string(opt.profile));


	if (opt.acctg_freq)
		setenvf(NULL, "SLURM_ACCTG_FREQ", "%s", opt.acctg_freq);

#ifdef HAVE_NATIVE_CRAY
	if (opt.network && opt.shared)
		fatal("Requesting network performance counters requires "
		      "exclusive access.  Please add the --exclusive option "
		      "to your request.");
	if (opt.network)
		setenv("SLURM_NETWORK", opt.network, 1);
#endif

	if (opt.mem_bind_type && (getenv("SBATCH_MEM_BIND") == NULL)) {
		char tmp[64];
		slurm_sprint_mem_bind_type(tmp, opt.mem_bind_type);
		if (opt.mem_bind) {
			xstrfmtcat(pack_env.mem_bind, "%s:%s",
				   tmp, opt.mem_bind);
		} else {
			pack_env.mem_bind = xstrdup(tmp);
		}
	}
	if (opt.mem_bind_type && (getenv("SLURM_MEM_BIND_SORT") == NULL) &&
	    (opt.mem_bind_type & MEM_BIND_SORT)) {
		pack_env.mem_bind_sort = xstrdup("sort");
	}

	if (opt.mem_bind_type && (getenv("SLURM_MEM_BIND_VERBOSE") == NULL)) {
		if (opt.mem_bind_type & MEM_BIND_VERBOSE) {
			pack_env.mem_bind_verbose = xstrdup("verbose");
		} else {
			pack_env.mem_bind_verbose = xstrdup("quiet");
		}
	}

	cpu_freq_set_env("SLURM_CPU_FREQ_REQ",
			 opt.cpu_freq_min, opt.cpu_freq_max, opt.cpu_freq_gov);

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

/* helper function for printing options
 *
 * warning: returns pointer to memory allocated on the stack.
 */
static char *print_constraints(void)
{
	char *buf = xstrdup("");

	if (opt.pn_min_cpus > 0)
		xstrfmtcat(buf, "mincpus=%d ", opt.pn_min_cpus);

	if (sbopt.minsockets > 0)
		xstrfmtcat(buf, "minsockets=%d ", sbopt.minsockets);

	if (sbopt.mincores > 0)
		xstrfmtcat(buf, "mincores=%d ", sbopt.mincores);

	if (sbopt.minthreads > 0)
		xstrfmtcat(buf, "minthreads=%d ", sbopt.minthreads);

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

/*
 * Return an absolute path for the "filename".  If "filename" is already
 * an absolute path, it returns a copy.  Free the returned with xfree().
 */
static void _fullpath(char **filename, const char *cwd)
{
	char *ptr = NULL;

	if ((*filename == NULL) || (*filename[0] == '/'))
		return;

	ptr = xstrdup(cwd);
	xstrcat(ptr, "/");
	xstrcat(ptr, *filename);
	xfree(*filename);
	*filename = ptr;
}

#define tf_(b) (b == true) ? "true" : "false"

static void _opt_list(void)
{
	char *str;

	info("defined options for program `%s'", opt.progname);
	info("----------------- ---------------------");

	info("user              : `%s'", opt.user);
	info("uid               : %ld", (long) opt.uid);
	info("gid               : %ld", (long) opt.gid);
	info("cwd               : %s", opt.cwd);
	info("ntasks            : %d %s", opt.ntasks,
	     opt.ntasks_set ? "(set)" : "(default)");
	if (opt.cpus_set)
		info("cpus_per_task     : %d", opt.cpus_per_task);
	if (opt.max_nodes) {
		info("nodes             : %d-%d",
		     opt.min_nodes, opt.max_nodes);
	} else {
		info("nodes             : %d %s", opt.min_nodes,
		     opt.nodes_set ? "(set)" : "(default)");
	}
	info("jobid             : %u %s", opt.jobid,
	     opt.jobid_set ? "(set)" : "(default)");
	info("partition         : %s",
	     opt.partition == NULL ? "default" : opt.partition);
	info("profile           : `%s'",
	     acct_gather_profile_to_string(opt.profile));
	info("job name          : `%s'", opt.job_name);
	info("reservation       : `%s'", opt.reservation);
	info("wckey             : `%s'", opt.wckey);
	info("distribution      : %s",
	     format_task_dist_states(opt.distribution));
	if ((opt.distribution  & SLURM_DIST_STATE_BASE) == SLURM_DIST_PLANE)
		info("plane size        : %u", opt.plane_size);
	info("verbose           : %d", opt.verbose);
	if (sbopt.requeue != NO_VAL)
		info("requeue           : %u", sbopt.requeue);
	info("overcommit        : %s", tf_(opt.overcommit));
	if (opt.time_limit == INFINITE)
		info("time_limit        : INFINITE");
	else if (opt.time_limit != NO_VAL)
		info("time_limit        : %d", opt.time_limit);
	if (opt.time_min != NO_VAL)
		info("time_min          : %d", opt.time_min);
	if (opt.nice)
		info("nice              : %d", opt.nice);
	info("account           : %s", opt.account);
	if (sbopt.batch_features)
		info("batch             : %s", sbopt.batch_features);
	info("comment           : %s", opt.comment);
	info("dependency        : %s", opt.dependency);
	if (opt.gres)
		info("gres              : %s", opt.gres);
	info("qos               : %s", opt.qos);
	str = print_constraints();
	info("constraints       : %s", str);
	xfree(str);
	info("reboot            : %s", opt.reboot ? "no" : "yes");
	info("network           : %s", opt.network);

	if (opt.begin) {
		char time_str[32];
		slurm_make_time_str(&opt.begin, time_str, sizeof(time_str));
		info("begin             : %s", time_str);
	}
	if (opt.deadline) {
		char time_str[32];
		slurm_make_time_str(&opt.deadline, time_str, sizeof(time_str));
		info("deadline          : %s", time_str);
	}
	info("array             : %s",
	     sbopt.array_inx == NULL ? "N/A" : sbopt.array_inx);
	info("cpu_freq_min      : %u", opt.cpu_freq_min);
	info("cpu_freq_max      : %u", opt.cpu_freq_max);
	info("cpu_freq_gov      : %u", opt.cpu_freq_gov);
	if (opt.delay_boot != NO_VAL)
		info("delay_boot        : %u", opt.delay_boot);
	info("mail_type         : %s", print_mail_type(opt.mail_type));
	info("mail_user         : %s", opt.mail_user);
	info("sockets-per-node  : %d", opt.sockets_per_node);
	info("cores-per-socket  : %d", opt.cores_per_socket);
	info("threads-per-core  : %d", opt.threads_per_core);
	info("ntasks-per-node   : %d", opt.ntasks_per_node);
	info("ntasks-per-socket : %d", opt.ntasks_per_socket);
	info("ntasks-per-core   : %d", opt.ntasks_per_core);
	info("mem-bind          : %s",
	     opt.mem_bind == NULL ? "default" : opt.mem_bind);
	info("plane_size        : %u", opt.plane_size);
	info("propagate         : %s",
	     sbopt.propagate == NULL ? "NONE" : sbopt.propagate);
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
	info("burst_buffer_file : `%s'", sbopt.burst_buffer_file);
	str = print_commandline(sbopt.script_argc, sbopt.script_argv);
	info("remote command    : `%s'", str);
	xfree(str);
	info("power             : %s", power_flags_str(opt.power_flags));
	info("wait              : %s", sbopt.wait ? "no" : "yes");
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
"Usage: sbatch [-N nnodes] [-n ntasks]\n"
"              [-c ncpus] [-r n] [-p partition] [--hold] [--parsable] [-t minutes]\n"
"              [-D path] [--no-kill] [--overcommit]\n"
"              [--input file] [--output file] [--error file]\n"
"              [--time-min=minutes] [--licenses=names] [--clusters=cluster_names]\n"
"              [--chdir=directory] [--oversubscibe] [-m dist] [-J jobname]\n"
"              [--jobid=id] [--verbose] [--gid=group] [--uid=user]\n"
"              [--contiguous] [--mincpus=n] [--mem=MB] [--tmp=MB] [-C list]\n"
"              [--account=name] [--dependency=type:jobid] [--comment=name]\n"
"              [--mail-type=type] [--mail-user=user][--nice[=value]] [--wait]\n"
"              [--requeue] [--no-requeue] [--ntasks-per-node=n] [--propagate]\n"
"              [--nodefile=file] [--nodelist=hosts] [--exclude=hosts]\n"
"              [--network=type] [--mem-per-cpu=MB] [--qos=qos] [--gres=list]\n"
"              [--mem-bind=...] [--reservation=name] [--mcs-label=mcs]\n"
"              [--cpu-freq=min[-max[:gov]] [--power=flags] [--gres-flags=opts]\n"
"              [--switches=max-switches{@max-time-to-wait}] [--reboot]\n"
"              [--core-spec=cores] [--thread-spec=threads]\n"
"              [--bb=burst_buffer_spec] [--bbf=burst_buffer_file]\n"
"              [--array=index_values] [--profile=...] [--ignore-pbs] [--spread-job]\n"
"              [--export[=names]] [--export-file=file|fd] [--delay-boot=mins]\n"
"              [--use-min-nodes]\n"
"              [--cpus-per-gpu=n] [--gpus=n] [--gpu-bind=...] [--gpu-freq=...]\n"
"              [--gpus-per-node=n] [--gpus-per-socket=n]  [--gpus-per-task=n]\n"
"              [--mem-per-gpu=MB]\n"
"              executable [args...]\n");
}

static void _help(void)
{
	slurm_ctl_conf_t *conf;

	printf (
"Usage: sbatch [OPTIONS...] executable [args...]\n"
"\n"
"Parallel run options:\n"
"  -a, --array=indexes         job array index values\n"
"  -A, --account=name          charge job to specified account\n"
"      --bb=<spec>             burst buffer specifications\n"
"      --bbf=<file_name>       burst buffer specification file\n"
"      --begin=time            defer job until HH:MM MM/DD/YY\n"
"      --comment=name          arbitrary comment\n"
"      --cpu-freq=min[-max[:gov]] requested cpu frequency (and governor)\n"
"  -c, --cpus-per-task=ncpus   number of cpus required per task\n"

"  -d, --dependency=type:jobid defer job until condition on jobid is satisfied\n"
"      --deadline=time         remove the job if no ending possible before\n"
"                              this deadline (start > (deadline - time[-min]))\n"
"      --delay-boot=mins       delay boot for desired node features\n"
"  -D, --chdir=directory       set working directory for batch script\n"
"  -e, --error=err             file for batch script's standard error\n"
"      --export[=names]        specify environment variables to export\n"
"      --export-file=file|fd   specify environment variables file or file\n"
"                              descriptor to export\n"
"      --get-user-env          load environment from local cluster\n"
"      --gid=group_id          group ID to run job as (user root only)\n"
"      --gres=list             required generic resources\n"
"      --gres-flags=opts       flags related to GRES management\n"
"  -H, --hold                  submit job in held state\n"
"      --ignore-pbs            Ignore #PBS options in the batch script\n"
"  -i, --input=in              file for batch script's standard input\n"
"      --jobid=id              run under already allocated job\n"
"  -J, --job-name=jobname      name of job\n"
"  -k, --no-kill               do not kill job on node failure\n"
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
"  -n, --ntasks=ntasks         number of tasks to run\n"
"      --nice[=value]          decrease scheduling priority by value\n"
"      --no-requeue            if set, do not permit the job to be requeued\n"
"      --ntasks-per-node=n     number of tasks to invoke on each node\n"
"  -N, --nodes=N               number of nodes on which to run (N = min[-max])\n"
"  -o, --output=out            file for batch script's standard output\n"
"  -O, --overcommit            overcommit resources\n"
"  -p, --partition=partition   partition requested\n"
"      --parsable              outputs only the jobid and cluster name (if present),\n"
"                              separated by semicolon, only on successful submission.\n"
"      --power=flags           power management options\n"
"      --priority=value        set the priority of the job to value\n"
"      --profile=value         enable acct_gather_profile for detailed data\n"
"                              value is all or none or any combination of\n"
"                              energy, lustre, network or task\n"
"      --propagate[=rlimits]   propagate all [or specific list of] rlimits\n"
"  -q, --qos=qos               quality of service\n"
"  -Q, --quiet                 quiet mode (suppress informational messages)\n"
"      --reboot                reboot compute nodes before starting job\n"
"      --requeue               if set, permit the job to be requeued\n"
"  -s, --oversubscribe         over subscribe resources with other jobs\n"
"  -S, --core-spec=cores       count of reserved cores\n"
"      --signal=[B:]num[@time] send signal when time limit within time seconds\n"
"      --spread-job            spread job across as many nodes as possible\n"
"      --switches=max-switches{@max-time-to-wait}\n"
"                              Optimum switches and max time to wait for optimum\n"
"      --thread-spec=threads   count of reserved threads\n"
"  -t, --time=minutes          time limit\n"
"      --time-min=minutes      minimum time limit (if distinct)\n"
"      --uid=user_id           user ID to run job as (user root only)\n"
"      --use-min-nodes         if a range of node counts is given, prefer the\n"
"                              smaller count\n"
"  -v, --verbose               verbose mode (multiple -v's increase verbosity)\n"
"  -W, --wait                  wait for completion of submitted job\n"
"      --wckey=wckey           wckey to run job under\n"
"      --wrap[=command string] wrap command string in a sh script and submit\n"

"\n"
"Constraint options:\n"
"      --cluster-constraint=[!]list specify a list of cluster constraints\n"
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
"  -u, --usage                 display brief usage message\n"
"\n"
"Other options:\n"
"  -V, --version               output version information and exit\n"
"\n"
		);

}

extern void init_envs(sbatch_env_t *local_env)
{
	local_env->cpus_per_task	= NO_VAL;
	local_env->dist			= NULL;
	local_env->dist_lllp		= NULL;
	local_env->mem_bind		= NULL;
	local_env->mem_bind_sort	= NULL;
	local_env->mem_bind_verbose	= NULL;
	local_env->ntasks		= NO_VAL;
	local_env->ntasks_per_core	= NO_VAL;
	local_env->ntasks_per_node	= NO_VAL;
	local_env->ntasks_per_socket	= NO_VAL;
	local_env->plane_size		= NO_VAL;
}

extern void set_envs(char ***array_ptr, sbatch_env_t *local_env,
		     int pack_offset)
{
	if ((local_env->cpus_per_task != NO_VAL) &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_CPUS_PER_TASK",
					  pack_offset, "%u",
					  local_env->cpus_per_task)) {
		error("Can't set SLURM_CPUS_PER_TASK env variable");
	}
	if (local_env->dist &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_DISTRIBUTION",
					  pack_offset, "%s",
					  local_env->dist)) {
		error("Can't set SLURM_DISTRIBUTION env variable");
	}
	if (local_env->mem_bind &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_MEM_BIND",
					  pack_offset, "%s",
					  local_env->mem_bind)) {
		error("Can't set SLURM_MEM_BIND env variable");
	}
	if (local_env->mem_bind_sort &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_MEM_BIND_SORT",
					  pack_offset, "%s",
					  local_env->mem_bind_sort)) {
		error("Can't set SLURM_MEM_BIND_SORT env variable");
	}
	if (local_env->mem_bind_verbose &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_MEM_BIND_VERBOSE",
					  pack_offset, "%s",
					  local_env->mem_bind_verbose)) {
		error("Can't set SLURM_MEM_BIND_VERBOSE env variable");
	}
	if (local_env->dist_lllp &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_DIST_LLLP",
					  pack_offset, "%s",
					  local_env->dist_lllp)) {
		error("Can't set SLURM_DIST_LLLP env variable");
	}
	if (local_env->ntasks != NO_VAL) {
		if (!env_array_overwrite_pack_fmt(array_ptr, "SLURM_NPROCS",
						  pack_offset, "%u",
						  local_env->ntasks))
			error("Can't set SLURM_NPROCS env variable");
		if (!env_array_overwrite_pack_fmt(array_ptr, "SLURM_NTASKS",
						  pack_offset, "%u",
						  local_env->ntasks))
			error("Can't set SLURM_NTASKS env variable");
	}
	if ((local_env->ntasks_per_core != NO_VAL) &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_NTASKS_PER_CORE",
					  pack_offset, "%u",
					  local_env->ntasks_per_core)) {
		error("Can't set SLURM_NTASKS_PER_CORE env variable");
	}
	if ((local_env->ntasks_per_node != NO_VAL) &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_NTASKS_PER_NODE",
					  pack_offset, "%u",
					  local_env->ntasks_per_node)) {
		error("Can't set SLURM_NTASKS_PER_NODE env variable");
	}
	if ((local_env->ntasks_per_socket != NO_VAL) &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_NTASKS_PER_SOCKET",
					  pack_offset, "%u",
					  local_env->ntasks_per_socket)) {
		error("Can't set SLURM_NTASKS_PER_SOCKET env variable");
	}
	if ((local_env->plane_size != NO_VAL) &&
	    !env_array_overwrite_pack_fmt(array_ptr, "SLURM_DIST_PLANESIZE",
					  pack_offset, "%u",
					  local_env->plane_size)) {
		error("Can't set SLURM_DIST_PLANESIZE env variable");
	}
}
