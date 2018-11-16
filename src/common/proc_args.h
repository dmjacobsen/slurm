/*****************************************************************************\
 *  proc_args.h - helper functions for command argument processing
 *****************************************************************************
 *  Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
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

#ifndef _PROC_ARGS_H
#define _PROC_ARGS_H

#include <sys/types.h>
#include <unistd.h>

#include "src/common/macros.h" /* true and false */
#include "src/common/env.h"
#include "src/common/slurm_opt.h"

/* convert task state ID to equivalent string */
extern char *format_task_dist_states(task_dist_states_t t);

/* print this version of Slurm */
void print_slurm_version(void);

/* print the available gres options */
void print_gres_help(void);

/* set distribution type strings from distribution type const */
void set_distribution(task_dist_states_t distribution,
		      char **dist, char **lllp_dist);

/* verify the requested distribution type */
task_dist_states_t verify_dist_type(const char *arg, uint32_t *plane_size);

/* return command name from its full path name */
char *base_name(const char *command);

/*
 * str_to_mbytes(): verify that arg is numeric with optional "K", "M", "G"
 * or "T" at end and return the number in mega-bytes. Default units are MB.
 */
long str_to_mbytes(const char *arg);

/*
 * str_to_mbytes2(): verify that arg is numeric with optional "K", "M", "G"
 * or "T" at end and return the number in mega-bytes. Default units are GB
 * if ???, otherwise MB.
 */
long str_to_mbytes2(const char *arg);

/* verify that a node count in arg is of a known form (count or min-max) */
bool verify_node_count(const char *arg, int *min_nodes, int *max_nodes);

/* verify a node list is valid based on the dist and task count given */
bool verify_node_list(char **node_list_pptr, enum task_dist_states dist,
		      int task_count);

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
			    int *max, bool isFatal);

/* verify resource counts from a complex form of: X, X:X, X:X:X or X:X:X:X */
bool verify_socket_core_thread_count(const char *arg, int *min_sockets,
				     int *min_cores, int *min_threads,
				     cpu_bind_type_t *cpu_bind_type);

/* verify a hint and convert it into the implied settings */
bool verify_hint(const char *arg, int *min_sockets, int *min_cores,
		 int *min_threads, int *ntasks_per_core,
		 cpu_bind_type_t *cpu_bind_type);

/* parse the mail type */
uint16_t parse_mail_type(const char *arg);

/* print the mail type */
char *print_mail_type(const uint16_t type);

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
		  bool test_exec);

/* helper function for printing options */
char *print_commandline(const int script_argc, char **script_argv);

/* Translate a signal option string "--signal=<int>[@<time>]" into
 * it's warn_signal and warn_time components.
 * RET 0 on success, -1 on failure */
int get_signal_opts(char *optarg, uint16_t *warn_signal, uint16_t *warn_time,
		    uint16_t *warn_flags);

/* Convert a signal name to it's numeric equivalent.
 * Return 0 on failure */
int sig_name2num(char *signal_name);

/*
 * parse_uint16/32/64 - Convert ascii string to a 16/32/64 bit unsigned int.
 * IN      aval - ascii string.
 * IN/OUT  ival - 16/32/64 bit pointer.
 * RET     0 if no error, 1 otherwise.
 */
extern int parse_uint16(char *aval, uint16_t *ival);
extern int parse_uint32(char *aval, uint32_t *ival);
extern int parse_uint64(char *aval, uint64_t *ival);

/* Get a decimal integer from arg
 * IN      name - command line name
 * IN      val - command line argument value
 * IN      positive - true if number needs to be greater than 0
 * RET     Returns the integer on success, exits program on failure.
 */
extern int parse_int(const char *name, const char *val, bool positive);


/* print_db_notok() - Print an error message about slurmdbd
 *                    is unreachable or wrong cluster name.
 * IN  cname - char * cluster name
 * IN  isenv - bool   cluster name from env or from command line option.
 */
extern void print_db_notok(const char *cname, bool isenv);

/*
 * parse_resv_flags() used to parse the Flags= option.  It handles
 * daily, weekly, static_alloc, part_nodes, and maint, optionally
 * preceded by + or -, separated by a comma but no spaces.
 *
 * flagstr IN - reservation flag string
 * msg IN - string to append to error message (e.g. function name)
 * RET equivalent reservation flag bits
 */
extern uint64_t parse_resv_flags(const char *flagstr, const char *msg);

extern uint16_t parse_compress_type(const char *arg);

extern int validate_acctg_freq(char *acctg_freq);

/*
 * Format a tres_per_* argument
 * dest OUT - resulting string
 * prefix IN - TRES type (e.g. "gpu")
 * src IN - user input, can include multiple comma-separated specifications
 */
extern void xfmt_tres(char **dest, char *prefix, char *src);

/*
 * Format a tres_freq argument
 * dest OUT - resulting string
 * prefix IN - TRES type (e.g. "gpu")
 * src IN - user input
 */
extern void xfmt_tres_freq(char **dest, char *prefix, char *src);

extern int arg_set_accel_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_account(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_acctg_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_array(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_batch(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_bb(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_bbf(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_bcast(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_begin(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_bell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_blrts_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_chdir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_checkpoint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_checkpoint_dir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cluster(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cluster_constraint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_clusters(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cnload_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_comment(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_compress(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_conn_type(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_constraint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_contiguous(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_core_spec(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cores_per_socket(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cpu_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cpu_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cpus_per_gpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cpus_per_task(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_cpus_per_task_int(slurm_opt_t *opt, int arg, const char *label, bool is_fatal);
extern int arg_set_deadline(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_debugger_test(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_delay_boot(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_dependency(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_disable_status(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_distribution(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_epilog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_error(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_exclude(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_exclusive(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_export(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_export_file(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_extra_node_info(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_geometry(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_get_user_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gpu_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gpu_freq(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gpus(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gpus_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gpus_per_task(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gres(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_gres_flags(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_hint(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_hold(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ignore_pbs(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_immediate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_input(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ioload_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_job_name(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_job_name_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_jobid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_join(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_kill_command(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_kill_on_bad_exit(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_kill_on_invalid_dep(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_label(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_launch_cmd(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_launcher_opts(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_licenses(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_linux_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mail_type(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mail_user(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mcs_label(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mem(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mem_mb(slurm_opt_t *opt, int64_t mbytes, const char *label);
extern int arg_set_mem_bind(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mem_per_cpu_mb(slurm_opt_t *opt, int64_t mbytes, const char *label);
extern int arg_set_mem_per_cpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mem_per_gpu(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mem_per_gpu_mb(slurm_opt_t *opt, int64_t mbytes, const char *label, bool is_fatal);
extern int arg_set_mincores(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mincpus(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mincpus_int(slurm_opt_t *opt, int arg, const char *label, bool is_fatal);
extern int arg_set_minsockets(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_minthreads(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mloader_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_mpi(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_msg_timeout(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_multi_prog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_network(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_network_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_nice(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_allocate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_bell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_kill(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_requeue(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_rotate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_no_shell(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_nodefile(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_nodelist(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_nodes_fromenv(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ntasks_int(slurm_opt_t *opt, int ntasks, const char *label);
extern int arg_set_ntasks(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ntasks_per_core(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ntasks_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ntasks_per_socket(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_open_mode(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_output(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_overcommit(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_oversubscribe(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_parsable(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_partition(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_power(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_preserve_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_preserve_slurm_env(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_priority(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_profile(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_prolog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_propagate(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_pty(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_qos(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_quiet(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_quit_on_interrupt(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_ramdisk_image(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_reboot(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_relative(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_requeue(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_reservation(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_restart_dir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_resv_ports(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_runjob_opts(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_share(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_signal(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_slurmd_debug(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_sockets_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_spread_job(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_switches(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_task_epilog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_task_prolog(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_tasks(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_tasks_per_node(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_test_only(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_thread_spec(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_threads(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_threads_per_core(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_time(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_time_min(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_tmp(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_tmp_mb(slurm_opt_t *opt, long mbytes, const char *label, bool is_fatal);
extern int arg_set_uid(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_umask(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_unbuffered(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_use_min_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_verbose(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_wait(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_wait_all_nodes(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_wckey(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_workdir(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_wrap(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
extern int arg_set_x11(slurm_opt_t *opt, const char *arg, const char *label, bool is_fatal);
#endif /* !_PROC_ARGS_H */
