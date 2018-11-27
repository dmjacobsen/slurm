#include "src/common/proc_args.h"
#include "src/common/slurm_opt.h"

#define LONG_OPT_ACCEL_BIND 0x100
#define LONG_OPT_BCAST 0x101
#define LONG_OPT_CHECKPOINT 0x102
#define LONG_OPT_CHECKPOINT_DIR 0x103
#define LONG_OPT_COMPRESS 0x104
#define LONG_OPT_CPU_BIND 0x105
#define LONG_OPT_DEBUG_TS 0x106
#define LONG_OPT_EPILOG 0x107
#define LONG_OPT_EXPORT 0x108
#define LONG_OPT_LAUNCH_CMD 0x109
#define LONG_OPT_LAUNCHER_OPTS 0x10a
#define LONG_OPT_MPI 0x10b
#define LONG_OPT_TIMEO 0x10c
#define LONG_OPT_MULTI 0x10d
#define LONG_OPT_OPEN_MODE 0x10e
#define LONG_OPT_PACK_GROUP 0x10f
#define LONG_OPT_PROLOG 0x110
#define LONG_OPT_PROPAGATE 0x111
#define LONG_OPT_PTY 0x112
#define LONG_OPT_QUIT_ON_INTR 0x113
#define LONG_OPT_RESTART_DIR 0x114
#define LONG_OPT_RESV_PORTS 0x115
#define LONG_OPT_DEBUG_SLURMD 0x116
#define LONG_OPT_TASK_EPILOG 0x117
#define LONG_OPT_TASK_PROLOG 0x118
#define LONG_OPT_TEST_ONLY 0x119
#define LONG_OPT_TRES_PER_JOB 0x11a
#define LONG_OPT_BELL 0x11b
#define LONG_OPT_NO_BELL 0x11c
#define LONG_OPT_NOSHELL 0x11d
#define LONG_OPT_WAIT_ALL_NODES 0x11e
#define LONG_OPT_BATCH 0x11f
#define LONG_OPT_EXPORT_FILE 0x120
#define LONG_OPT_IGNORE_PBS 0x121
#define LONG_OPT_KILL_INV_DEP 0x122
#define LONG_OPT_NO_REQUEUE 0x123
#define LONG_OPT_PARSABLE 0x124
#define LONG_OPT_REQUEUE 0x125
#define LONG_OPT_WRAP 0x126
#define LONG_OPT_ACCTG_FREQ 0x127
#define LONG_OPT_BURST_BUFFER_SPEC 0x128
#define LONG_OPT_BURST_BUFFER_FILE 0x129
#define LONG_OPT_BEGIN 0x12a
#define LONG_OPT_CLUSTER_CONSTRAINT 0x12b
#define LONG_OPT_COMMENT 0x12c
#define LONG_OPT_CONT 0x12d
#define LONG_OPT_CORESPERSOCKET 0x12e
#define LONG_OPT_CPU_FREQ 0x12f
#define LONG_OPT_CPUS_PER_GPU 0x130
#define LONG_OPT_DEADLINE 0x131
#define LONG_OPT_DELAY_BOOT 0x132
#define LONG_OPT_EXCLUSIVE 0x133
#define LONG_OPT_GET_USER_ENV 0x134
#define LONG_OPT_GID 0x135
#define LONG_OPT_GPU_BIND 0x136
#define LONG_OPT_GPU_FREQ 0x137
#define LONG_OPT_GPUS_PER_NODE 0x138
#define LONG_OPT_GPUS_PER_SOCKET 0x139
#define LONG_OPT_GPUS_PER_TASK 0x13a
#define LONG_OPT_GRES 0x13b
#define LONG_OPT_GRES_FLAGS 0x13c
#define LONG_OPT_HINT 0x13d
#define LONG_OPT_JOBID 0x13e
#define LONG_OPT_MAIL_TYPE 0x13f
#define LONG_OPT_MAIL_USER 0x140
#define LONG_OPT_MCS_LABEL 0x141
#define LONG_OPT_MEM 0x142
#define LONG_OPT_MEM_BIND 0x143
#define LONG_OPT_MEM_PER_CPU 0x144
#define LONG_OPT_MEM_PER_GPU 0x145
#define LONG_OPT_MINCORES 0x146
#define LONG_OPT_MINCPUS 0x147
#define LONG_OPT_MINCPU 0x148
#define LONG_OPT_MINSOCKETS 0x149
#define LONG_OPT_MINTHREADS 0x14a
#define LONG_OPT_NETWORK 0x14b
#define LONG_OPT_NICE 0x14c
#define LONG_OPT_NTASKSPERCORE 0x14d
#define LONG_OPT_NTASKSPERNODE 0x14e
#define LONG_OPT_NTASKSPERSOCKET 0x14f
#define LONG_OPT_POWER 0x150
#define LONG_OPT_PRIORITY 0x151
#define LONG_OPT_PROFILE 0x152
#define LONG_OPT_REBOOT 0x153
#define LONG_OPT_RESERVATION 0x154
#define LONG_OPT_SIGNAL 0x155
#define LONG_OPT_SOCKETSPERNODE 0x156
#define LONG_OPT_SPREAD_JOB 0x157
#define LONG_OPT_REQ_SWITCH 0x158
#define LONG_OPT_THREAD_SPEC 0x159
#define LONG_OPT_THREADSPERCORE 0x15a
#define LONG_OPT_TIME_MIN 0x15b
#define LONG_OPT_TMP 0x15c
#define LONG_OPT_UID 0x15d
#define LONG_OPT_USAGE 0x15e
#define LONG_OPT_USE_MIN_NODES 0x15f
#define LONG_OPT_WCKEY 0x160
#define LONG_OPT_X11 0x161

static struct slurm_long_option opt_common_account = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "account",
	.get_func  = &arg_get_account,
	.set_func  = &arg_set_account,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'A',
	.help_short = "name",
	.help_long = "charge job to specified account",
};

static struct slurm_long_option opt_common_account_deprecated = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "account",
	.get_func  = &arg_get_account,
	.set_func  = &arg_set_account,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'U',
	.help_short = "name",
	.help_long = "charge job to specified account",
};

static struct slurm_long_option opt_common_bb = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "bb",
	.get_func  = &arg_get_bb,
	.set_func  = &arg_set_bb,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_BURST_BUFFER_SPEC,
	.help_short = "<spec>",
	.help_long = "burst buffer specifications",
};

static struct slurm_long_option opt_common_bbf = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "bbf",
	.get_func  = &arg_get_bbf,
	.set_func  = &arg_set_bbf,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_BURST_BUFFER_FILE,
	.help_short = "<file_name>",
	.help_long = "burst buffer specification file",
};

static struct slurm_long_option opt_common_begin = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "begin",
	.get_func  = &arg_get_begin,
	.set_func  = &arg_set_begin,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_BEGIN,
	.help_short = "time",
	.help_long = "defer job until HH:MM MM/DD/YY",
};

static struct slurm_long_option opt_common_comment = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "comment",
	.get_func  = &arg_get_comment,
	.set_func  = &arg_set_comment,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_COMMENT,
	.help_short = "name",
	.help_long = "arbitrary comment",
};

static struct slurm_long_option opt_common_constraint = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "constraint",
	.get_func  = &arg_get_constraint,
	.set_func  = &arg_set_constraint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'C',
	.help_short = "list",
	.help_long = "specify a list of constraints",
};

static struct slurm_long_option opt_common_contiguous = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "contiguous",
	.get_func  = &arg_get_contiguous,
	.set_func  = &arg_set_contiguous,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_CONT,
	.help_long = "demand a contiguous range of nodes",
};

static struct slurm_long_option opt_common_core_spec = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "core-spec",
	.get_func  = &arg_get_core_spec,
	.set_func  = &arg_set_core_spec,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'S',
	.help_short = "cores",
	.help_long = "count of reserved cores",
};

static struct slurm_long_option opt_common_cpu_freq = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "cpu-freq",
	.get_func  = &arg_get_cpu_freq,
	.set_func  = &arg_set_cpu_freq,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CPU_FREQ,
	.help_short = "min[-max[:gov]]",
	.help_long = "requested cpu frequency (and governor)",
};

static struct slurm_long_option opt_common_cpus_per_gpu = {
	.opt_group = OPT_GRP_GPU,
	.name      = "cpus-per-gpu",
	.get_func  = &arg_get_cpus_per_gpu,
	.set_func  = &arg_set_cpus_per_gpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CPUS_PER_GPU,
	.help_short = "n",
	.help_long = "number of CPUs required per allocated GPU",
};

static struct slurm_long_option opt_common_cpus_per_task = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "cpus-per-task",
	.get_func  = &arg_get_cpus_per_task,
	.set_func  = &arg_set_cpus_per_task,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'c',
	.help_short = "ncpus",
	.help_long = "number of cpus required per task",
};

static struct slurm_long_option opt_common_deadline = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "deadline",
	.get_func  = &arg_get_deadline,
	.set_func  = &arg_set_deadline,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_DEADLINE,
	.help_short = "time",
	.help_long = "remove the job if no ending possible before this deadline (start > (deadline - time[-min]))",
};

static struct slurm_long_option opt_common_delay_boot = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "delay-boot",
	.get_func  = &arg_get_delay_boot,
	.set_func  = &arg_set_delay_boot,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_DELAY_BOOT,
	.help_short = "mins",
	.help_long = "delay boot for desired node features",
};

static struct slurm_long_option opt_common_dependency = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "dependency",
	.get_func  = &arg_get_dependency,
	.set_func  = &arg_set_dependency,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'd',
	.help_short = "type:jobid",
	.help_long = "defer job until condition on jobid is satisfied",
};

static struct slurm_long_option opt_common_dependency_deprecated = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "dependency_deprecated",
	.get_func  = &arg_get_dependency,
	.set_func  = &arg_set_dependency,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'P',
};


static struct slurm_long_option opt_common_distribution = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "distribution",
	.get_func  = &arg_get_distribution,
	.set_func  = &arg_set_distribution,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'm',
	.help_short = "type",
	.help_long = "distribution method for processes to nodes (type = block|cyclic|arbitrary)",
};

static struct slurm_long_option opt_common_exclude = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "exclude",
	.get_func  = &arg_get_exclude,
	.set_func  = &arg_set_exclude,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'x',
	.help_short = "hosts...",
	.help_long = "exclude a specific list of hosts",
};

static struct slurm_long_option opt_common_gpu_bind = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpu-bind",
	.get_func  = &arg_get_gpu_bind,
	.set_func  = &arg_set_gpu_bind,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GPU_BIND,
	.help_short = "...",
	.help_long = "task to gpu binding options",
};

static struct slurm_long_option opt_common_gpu_freq = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpu-freq",
	.get_func  = &arg_get_gpu_freq,
	.set_func  = &arg_set_gpu_freq,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GPU_FREQ,
	.help_short = "...",
	.help_long = "frequency and voltage of GPUs",
};

static struct slurm_long_option opt_common_gpus = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpus",
	.get_func  = &arg_get_gpus,
	.set_func  = &arg_set_gpus,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'G',
	.help_short = "n",
	.help_long = "count of GPUs required for the job",
};

static struct slurm_long_option opt_common_gpus_per_node = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpus-per-node",
	.get_func  = &arg_get_gpus_per_node,
	.set_func  = &arg_set_gpus_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GPUS_PER_NODE,
	.help_short = "n",
	.help_long = "number of GPUs required per allocated node",
};

static struct slurm_long_option opt_common_gpus_per_socket = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpus-per-socket",
	.get_func  = &arg_get_gpus_per_socket,
	.set_func  = &arg_set_gpus_per_socket,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GPUS_PER_SOCKET,
	.help_short = "n",
	.help_long = "number of GPUs required per allocated socket",
};

static struct slurm_long_option opt_common_gpus_per_task = {
	.opt_group = OPT_GRP_GPU,
	.name      = "gpus-per-task",
	.get_func  = &arg_get_gpus_per_task,
	.set_func  = &arg_set_gpus_per_task,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GPUS_PER_TASK,
	.help_short = "n",
	.help_long = "number of GPUs required per spawned task",
};

static struct slurm_long_option opt_common_gres = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "gres",
	.get_func  = &arg_get_gres,
	.set_func  = &arg_set_gres,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GRES,
	.help_short = "list",
	.help_long = "required generic resources",
};

static struct slurm_long_option opt_common_gres_flags = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "gres-flags",
	.get_func  = &arg_get_gres_flags,
	.set_func  = &arg_set_gres_flags,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GRES_FLAGS,
	.help_short = "opts",
	.help_long = "flags related to GRES management",
};

static struct slurm_long_option opt_common_hold = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "hold",
	.get_func  = &arg_get_hold,
	.set_func  = &arg_set_hold,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'H',
	.help_long = "submit job in held state",
};

static struct slurm_long_option opt_common_job_name = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "job-name",
	.get_func  = &arg_get_job_name,
	.set_func  = &arg_set_job_name,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'J',
	.help_short = "jobname",
	.help_long = "name of job",
};

static struct slurm_long_option opt_common_licenses = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "licenses",
	.get_func  = &arg_get_licenses,
	.set_func  = &arg_set_licenses,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'L',
	.help_short = "names",
	.help_long = "required license, comma separated",
};

static struct slurm_long_option opt_common_mail_type = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "mail-type",
	.get_func  = &arg_get_mail_type,
	.set_func  = &arg_set_mail_type,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MAIL_TYPE,
	.help_short = "type",
	.help_long = "notify on state change: BEGIN, END, FAIL or ALL",
};

static struct slurm_long_option opt_common_mail_user = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "mail-user",
	.get_func  = &arg_get_mail_user,
	.set_func  = &arg_set_mail_user,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MAIL_USER,
	.help_short = "user",
	.help_long = "who to send email notification for job state changes",
};

static struct slurm_long_option opt_common_mcs_label = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "mcs-label",
	.get_func  = &arg_get_mcs_label,
	.set_func  = &arg_set_mcs_label,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MCS_LABEL,
	.help_short = "mcs",
	.help_long = "mcs label if mcs plugin mcs/group is used",
};

static struct slurm_long_option opt_common_mem = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "mem",
	.get_func  = &arg_get_mem,
	.set_func  = &arg_set_mem,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM,
	.help_short = "MB",
	.help_long = "minimum amount of real memory",
};

static struct slurm_long_option opt_common_mincores = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "mincores",
	.get_func  = &arg_get_mincores,
	.set_func  = &arg_set_mincores,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINCORES,
};

static struct slurm_long_option opt_common_minsockets = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "minsockets",
	.get_func  = &arg_get_minsockets,
	.set_func  = &arg_set_minsockets,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINSOCKETS,
};

static struct slurm_long_option opt_common_minthreads = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "minthreads",
	.get_func  = &arg_get_minthreads,
	.set_func  = &arg_set_minthreads,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINTHREADS,
};

static struct slurm_long_option opt_common_nice = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "nice",
	.get_func  = &arg_get_nice,
	.set_func  = &arg_set_nice,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_NICE,
	.help_short = "value",
	.help_long = "decrease scheduling priority by value",
};

static struct slurm_long_option opt_common_no_kill = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "no-kill",
	.get_func  = &arg_get_no_kill,
	.set_func  = &arg_set_no_kill,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = 'k',
	.help_short = "on|off",
	.help_long = "do not kill job on node failure",
};

static struct slurm_long_option opt_common_nodelist = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "nodelist",
	.get_func  = &arg_get_nodelist,
	.set_func  = &arg_set_nodelist,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'w',
	.help_short = "hosts...",
	.help_long = "request a specific list of hosts",
};

static struct slurm_long_option opt_common_nodes = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "nodes",
	.get_func  = &arg_get_nodes,
	.set_func  = &arg_set_nodes,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'N',
	.help_short = "N",
	.help_long = "number of nodes on which to run (N = min[-max])",
};

static struct slurm_long_option opt_common_ntasks_per_core = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "ntasks-per-core",
	.get_func  = &arg_get_ntasks_per_core,
	.set_func  = &arg_set_ntasks_per_core,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERCORE,
	.help_short = "n",
	.help_long = "number of tasks to invoke on each core",
};

static struct slurm_long_option opt_common_ntasks_per_socket = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "ntasks-per-socket",
	.get_func  = &arg_get_ntasks_per_socket,
	.set_func  = &arg_set_ntasks_per_socket,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERSOCKET,
	.help_short = "n",
	.help_long = "number of tasks to invoke on each socket",
};

static struct slurm_long_option opt_common_overcommit = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "overcommit",
	.get_func  = &arg_get_overcommit,
	.set_func  = &arg_set_overcommit,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'O',
	.help_long = "overcommit resources",
};

static struct slurm_long_option opt_common_partition = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "partition",
	.get_func  = &arg_get_partition,
	.set_func  = &arg_set_partition,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'p',
	.help_short = "partition",
	.help_long = "partition requested",
};

static struct slurm_long_option opt_common_power = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "power",
	.get_func  = &arg_get_power,
	.set_func  = &arg_set_power,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_POWER,
	.help_short = "flags",
	.help_long = "power management options",
};

static struct slurm_long_option opt_common_priority = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "priority",
	.get_func  = &arg_get_priority,
	.set_func  = &arg_set_priority,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_PRIORITY,
	.help_short = "value",
	.help_long = "set the priority of the job to value",
};

static struct slurm_long_option opt_common_profile = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "profile",
	.get_func  = &arg_get_profile,
	.set_func  = &arg_set_profile,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_PROFILE,
	.help_short = "value",
	.help_long = "enable acct_gather_profile for detailed data value is all or none or any combination of energy, lustre, network or task",
};

static struct slurm_long_option opt_common_qos = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "qos",
	.get_func  = &arg_get_qos,
	.set_func  = &arg_set_qos,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'q',
	.help_short = "qos",
	.help_long = "quality of service",
};

static struct slurm_long_option opt_common_reservation = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "reservation",
	.get_func  = &arg_get_reservation,
	.set_func  = &arg_set_reservation,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_RESERVATION,
	.help_short = "name",
	.help_long = "allocate resources from named reservation",
};

static struct slurm_long_option opt_common_share = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "share",
	.get_func  = &arg_get_share,
	.set_func  = &arg_set_share,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 's',
};

static struct slurm_long_option opt_common_signal = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "signal",
	.get_func  = &arg_get_signal,
	.set_func  = &arg_set_signal,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_SIGNAL,
	.help_short = "[B:]num[@time]",
	.help_long = "send signal when time limit within time seconds",
};

static struct slurm_long_option opt_common_spread_job = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "spread-job",
	.get_func  = &arg_get_spread_job,
	.set_func  = &arg_set_spread_job,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_SPREAD_JOB,
	.help_long = "spread job across as many nodes as possible",
};

static struct slurm_long_option opt_common_time = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "time",
	.get_func  = &arg_get_time,
	.set_func  = &arg_set_time,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 't',
	.help_short = "minutes",
	.help_long = "time limit",
};

static struct slurm_long_option opt_common_time_min = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "time-min",
	.get_func  = &arg_get_time_min,
	.set_func  = &arg_set_time_min,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TIME_MIN,
	.help_short = "minutes",
	.help_long = "minimum time limit (if distinct)",
};

static struct slurm_long_option opt_common_tmp = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "tmp",
	.get_func  = &arg_get_tmp,
	.set_func  = &arg_set_tmp,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TMP,
	.help_short = "MB",
	.help_long = "minimum amount of temporary disk",
};

static struct slurm_long_option opt_common_use_min_nodes = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "use-min-nodes",
	.get_func  = &arg_get_use_min_nodes,
	.set_func  = &arg_set_use_min_nodes,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_USE_MIN_NODES,
	.help_long = "if a range of node counts is given, prefer the smaller count",
};

static struct slurm_long_option opt_common_wckey = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "wckey",
	.get_func  = &arg_get_wckey,
	.set_func  = &arg_set_wckey,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_WCKEY,
	.help_short = "wckey",
	.help_long = "wckey to run job under",
};

static struct slurm_long_option opt_common_x11 = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "x11",
	.get_func  = &arg_get_x11,
	.set_func  = &arg_set_x11,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_X11,
};

static struct slurm_long_option opt_salloc_acctg_freq = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "acctg-freq",
	.get_func  = &arg_get_acctg_freq,
	.set_func  = &arg_set_acctg_freq,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_ACCTG_FREQ,
};

static struct slurm_long_option opt_salloc_bell = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "bell",
	.get_func  = &arg_get_bell,
	.set_func  = &arg_set_bell,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_BELL,
	.help_long = "ring the terminal bell when the job is allocated",
};

static struct slurm_long_option opt_salloc_chdir = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "chdir",
	.get_func  = &arg_get_chdir,
	.set_func  = &arg_set_chdir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'D',
	.help_short = "path",
	.help_long = "change working directory",
};

static struct slurm_long_option opt_salloc_cluster = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "cluster",
	.get_func  = &arg_get_cluster,
	.set_func  = &arg_set_cluster,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
};

static struct slurm_long_option opt_salloc_cluster_constraint = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "cluster-constraint",
	.get_func  = &arg_get_cluster_constraint,
	.set_func  = &arg_set_cluster_constraint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CLUSTER_CONSTRAINT,
	.help_short = "list",
	.help_long = "specify a list of cluster constraints",
};

static struct slurm_long_option opt_salloc_clusters = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "clusters",
	.get_func  = &arg_get_cluster,
	.set_func  = &arg_set_cluster,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
	.help_short = "names",
	.help_long = "Comma separated list of clusters to issue commands to. Default is current cluster. Name of 'all' will submit to run on all clusters. NOTE: SlurmDBD must up.",
};

static struct slurm_long_option opt_salloc_cores_per_socket = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "cores-per-socket",
	.get_func  = &arg_get_cores_per_socket,
	.set_func  = &arg_set_cores_per_socket,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CORESPERSOCKET,
	.help_short = "C",
	.help_long = "number of cores per socket to allocate",
};

static struct slurm_long_option opt_salloc_exclusive = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "exclusive",
	.get_func  = &arg_get_exclusive,
	.set_func  = &arg_set_exclusive,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_EXCLUSIVE,
	.help_short = "user|mcs",
	.help_long = "allocate nodes in exclusive mode when cpu consumable resource is enabled. Optional flag \"user\" ensures nodes only share jobs from the same user. Optional flag \"mcs\" ensures nodes only share jobs from the same MCS group.",
};

static struct slurm_long_option opt_salloc_extra_node_info = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "extra-node-info",
	.get_func  = &arg_get_extra_node_info,
	.set_func  = &arg_set_extra_node_info,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'B',
	.help_short = "S[:C[:T]]",
	.help_long = "Expands to: S=sockets-per-node, C=cores-per-socket, T=threads-per-core. Each field can be 'min' or wildcard '*'; total cpus requested = (N x S x C x T)",
};

static struct slurm_long_option opt_salloc_get_user_env = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "get-user-env",
	.get_func  = &arg_get_get_user_env,
	.set_func  = &arg_set_get_user_env,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_GET_USER_ENV,
	.help_long = "used by Moab.  See srun man page.",
};

static struct slurm_long_option opt_salloc_gid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "gid",
	.get_func  = &arg_get_gid,
	.set_func  = &arg_set_gid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GID,
	.help_short = "group_id",
	.help_long = "group ID to run job as (user root only)",
};

static struct slurm_long_option opt_salloc_help = {
	.opt_group = OPT_GRP_HELP,
	.name      = "help",
	.get_func  = NULL,
	.set_func  = &arg_help,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'h',
	.help_long = "show this help message",
};

static struct slurm_long_option opt_salloc_hint = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "hint",
	.get_func  = &arg_get_hint,
	.set_func  = &arg_set_hint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_HINT,
	.help_long = "Bind tasks according to application hints (see \"--hint=help\" for options)",
};

static struct slurm_long_option opt_salloc_immediate = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "immediate",
	.get_func  = &arg_get_immediate,
	.set_func  = &arg_set_immediate,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = 'I',
	.help_short = "secs",
	.help_long = "exit if resources not available in \"secs\"",
};

static struct slurm_long_option opt_salloc_jobid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "jobid",
	.get_func  = &arg_get_jobid,
	.set_func  = &arg_set_jobid,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_JOBID,
	.help_short = "id",
	.help_long = "specify jobid to use",
};

static struct slurm_long_option opt_salloc_kill_command = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "kill-command",
	.get_func  = &arg_get_kill_command,
	.set_func  = &arg_set_kill_command,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = 'K',
	.help_short = "signal",
	.help_long = "signal to send terminating job",
};

static struct slurm_long_option opt_salloc_mem_bind = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "mem_bind",
	.get_func  = &arg_get_mem_bind,
	.set_func  = &arg_set_mem_bind,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_BIND,
};

static struct slurm_long_option opt_salloc_mem_per_cpu = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "mem-per-cpu",
	.get_func  = &arg_get_mem_per_cpu,
	.set_func  = &arg_set_mem_per_cpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_CPU,
	.help_short = "MB",
	.help_long = "maximum amount of real memory per allocated cpu required by the job.  --mem >= --mem-per-cpu if --mem is specified.",
};

static struct slurm_long_option opt_salloc_mem_per_gpu = {
	.opt_group = OPT_GRP_GPU,
	.name      = "mem-per-gpu",
	.get_func  = &arg_get_mem_per_gpu,
	.set_func  = &arg_set_mem_per_gpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_GPU,
	.help_short = "n",
	.help_long = "real memory required per allocated GPU",
};

static struct slurm_long_option opt_salloc_mincpus = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "mincpus",
	.get_func  = &arg_get_mincpus,
	.set_func  = &arg_set_mincpus,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINCPU,
	.help_short = "n",
	.help_long = "minimum number of logical processors (threads) per node",
};

static struct slurm_long_option opt_salloc_network = {
	.opt_group = OPT_GRP_CRAY,
	.name      = "network",
	.get_func  = &arg_get_network,
	.set_func  = &arg_set_network,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NETWORK,
	.help_short = "type",
	.help_long = "Use network performance counters (system, network, or processor)",
};

static struct slurm_long_option opt_salloc_no_bell = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "no-bell",
	.get_func  = &arg_get_no_bell,
	.set_func  = &arg_set_no_bell,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_NO_BELL,
	.help_long = "do NOT ring the terminal bell",
};

static struct slurm_long_option opt_salloc_no_shell = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "no-shell",
	.get_func  = &arg_get_no_shell,
	.set_func  = &arg_set_no_shell,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_NOSHELL,
};

static struct slurm_long_option opt_salloc_nodefile = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "nodefile",
	.get_func  = &arg_get_nodefile,
	.set_func  = &arg_set_nodefile,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'F',
	.help_short = "filename",
	.help_long = "request a specific list of hosts",
};

static struct slurm_long_option opt_salloc_ntasks = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks",
	.get_func  = &arg_get_ntasks,
	.set_func  = &arg_set_ntasks,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'n',
	.help_short = "N",
	.help_long = "number of processors required",
};

static struct slurm_long_option opt_salloc_ntasks_per_node = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks-per-node",
	.get_func  = &arg_get_tasks_per_node,
	.set_func  = &arg_set_tasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
	.help_short = "n",
	.help_long = "number of tasks to invoke on each node",
};

static struct slurm_long_option opt_salloc_oversubscribe = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "oversubscribe",
	.get_func  = &arg_get_share,
	.set_func  = &arg_set_share,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 's',
	.help_long = "oversubscribe resources with other jobs",
};

static struct slurm_long_option opt_salloc_quiet = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "quiet",
	.get_func  = &arg_get_quiet,
	.set_func  = &arg_set_quiet,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'Q',
	.help_long = "quiet mode (suppress informational messages)",
};

static struct slurm_long_option opt_salloc_reboot = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "reboot",
	.get_func  = &arg_get_reboot,
	.set_func  = &arg_set_reboot,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_REBOOT,
	.help_long = "reboot compute nodes before starting job",
};

static struct slurm_long_option opt_salloc_sockets_per_node = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "sockets-per-node",
	.get_func  = &arg_get_sockets_per_node,
	.set_func  = &arg_set_sockets_per_node,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_SOCKETSPERNODE,
	.help_short = "S",
	.help_long = "number of sockets per node to allocate",
};

static struct slurm_long_option opt_salloc_switches = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "switches",
	.get_func  = &arg_get_switches,
	.set_func  = &arg_set_switches,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_REQ_SWITCH,
	.help_short = "max-switches[@max-time-to-wait]",
	.help_long = "Optimum switches and max time to wait for optimum",
};

static struct slurm_long_option opt_salloc_tasks = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tasks",
	.get_func  = &arg_get_ntasks,
	.set_func  = &arg_set_ntasks,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'n',
};

static struct slurm_long_option opt_salloc_tasks_per_node = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tasks-per-node",
	.get_func  = &arg_get_tasks_per_node,
	.set_func  = &arg_set_tasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
};

static struct slurm_long_option opt_salloc_thread_spec = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "thread-spec",
	.get_func  = &arg_get_thread_spec,
	.set_func  = &arg_set_thread_spec,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREAD_SPEC,
	.help_short = "threads",
	.help_long = "count of reserved threads",
};

static struct slurm_long_option opt_salloc_threads_per_core = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "threads-per-core",
	.get_func  = &arg_get_threads_per_core,
	.set_func  = &arg_set_threads_per_core,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREADSPERCORE,
	.help_short = "T",
	.help_long = "number of threads per core to allocate",
};

static struct slurm_long_option opt_salloc_uid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "uid",
	.get_func  = &arg_get_uid,
	.set_func  = &arg_set_uid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_UID,
	.help_short = "user_id",
	.help_long = "user ID to run job as (user root only)",
};

static struct slurm_long_option opt_salloc_usage = {
	.opt_group = OPT_GRP_HELP,
	.name      = "usage",
	.get_func  = NULL,
	.set_func  = &arg_usage,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'u',
	.help_long = "display brief usage message",
};

static struct slurm_long_option opt_salloc_verbose = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "verbose",
	.get_func  = &arg_get_verbose,
	.set_func  = &arg_set_verbose,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'v',
	.help_long = "verbose mode (multiple -v's increase verbosity)",
};

static struct slurm_long_option opt_salloc_version = {
	.opt_group = OPT_GRP_OTHER,
	.name      = "version",
	.get_func  = NULL,
	.set_func  = &arg_version,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'V',
	.help_long = "output version information and exit",
};

static struct slurm_long_option opt_salloc_wait = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "wait",
	.get_func  = &arg_get_wait,
	.set_func  = &arg_set_wait,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'W',
};

static struct slurm_long_option opt_salloc_wait_all_nodes = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "wait-all-nodes",
	.get_func  = &arg_get_wait_all_nodes,
	.set_func  = &arg_set_wait_all_nodes,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_WAIT_ALL_NODES,
};

static struct slurm_long_option opt_sbatch_acctg_freq = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "acctg-freq",
	.get_func  = &arg_get_acctg_freq,
	.set_func  = &arg_set_acctg_freq,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_ACCTG_FREQ,
};

static struct slurm_long_option opt_sbatch_array = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "array",
	.get_func  = &arg_get_array,
	.set_func  = &arg_set_array,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'a',
	.help_short = "indexes",
	.help_long = "job array index values",
};

static struct slurm_long_option opt_sbatch_batch = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "batch",
	.get_func  = &arg_get_batch,
	.set_func  = &arg_set_batch,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_BATCH,
};

static struct slurm_long_option opt_sbatch_chdir = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "chdir",
	.get_func  = &arg_get_workdir,
	.set_func  = &arg_set_workdir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'D',
	.help_short = "directory",
	.help_long = "set working directory for batch script",
};

static struct slurm_long_option opt_sbatch_checkpoint = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "checkpoint",
	.get_func  = &arg_get_checkpoint,
	.set_func  = &arg_set_checkpoint,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CHECKPOINT,
};

static struct slurm_long_option opt_sbatch_checkpoint_dir = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "checkpoint-dir",
	.get_func  = &arg_get_checkpoint_dir,
	.set_func  = &arg_set_checkpoint_dir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CHECKPOINT_DIR,
};

static struct slurm_long_option opt_sbatch_cluster = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "cluster",
	.get_func  = &arg_get_clusters,
	.set_func  = &arg_set_clusters,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
};

static struct slurm_long_option opt_sbatch_cluster_constraint = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "cluster-constraint",
	.get_func  = &arg_get_cluster_constraint,
	.set_func  = &arg_set_cluster_constraint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CLUSTER_CONSTRAINT,
	.help_short = "[!]list",
	.help_long = "specify a list of cluster constraints",
};

static struct slurm_long_option opt_sbatch_clusters = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "clusters",
	.get_func  = &arg_get_clusters,
	.set_func  = &arg_set_clusters,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
	.help_short = "names",
	.help_long = "Comma separated list of clusters to issue commands to.  Default is current cluster.  Name of 'all' will submit to run on all clusters.  NOTE: SlurmDBD must up.",
};

static struct slurm_long_option opt_sbatch_cores_per_socket = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "cores-per-socket",
	.get_func  = &arg_get_cores_per_socket,
	.set_func  = &arg_set_cores_per_socket,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CORESPERSOCKET,
	.help_short = "C",
	.help_long = "number of cores per socket to allocate",
};

static struct slurm_long_option opt_sbatch_error = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "error",
	.get_func  = &arg_get_error,
	.set_func  = &arg_set_error,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'e',
	.help_short = "err",
	.help_long = "file for batch script's standard error",
};

static struct slurm_long_option opt_sbatch_exclusive = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "exclusive",
	.get_func  = &arg_get_exclusive,
	.set_func  = &arg_set_exclusive,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_EXCLUSIVE,
	.help_short = "user|mcs",
	.help_long = "allocate nodes in exclusive mode when cpu consumable resource is enabled. Optional flag \"user\" ensures nodes only share jobs from the same user. Optional flag \"mcs\" ensures nodes only share jobs from the same MCS group.",
};

static struct slurm_long_option opt_sbatch_export = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "export",
	.get_func  = &arg_get_export,
	.set_func  = &arg_set_export,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_EXPORT,
	.help_short = "names",
	.help_long = "specify environment variables to export",
};

static struct slurm_long_option opt_sbatch_export_file = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "export-file",
	.get_func  = &arg_get_export_file,
	.set_func  = &arg_set_export_file,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_EXPORT_FILE,
	.help_short = "file|fd",
	.help_long = "specify environment variables file or file descriptor to export",
};

static struct slurm_long_option opt_sbatch_extra_node_info = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "extra-node-info",
	.get_func  = &arg_get_extra_node_info,
	.set_func  = &arg_set_extra_node_info,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'B',
	.help_short = "S[:C[:T]]",
	.help_long = "Expands to: S=sockets-per-node, C=cores-per-socket, T=threads-per-core. Each field can be 'min' or wildcard '*'; total cpus requested = (N x S x C x T)",
};

static struct slurm_long_option opt_sbatch_get_user_env = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "get-user-env",
	.get_func  = &arg_get_get_user_env,
	.set_func  = &arg_set_get_user_env,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_GET_USER_ENV,
	.help_long = "load environment from local cluster",
};

static struct slurm_long_option opt_sbatch_gid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "gid",
	.get_func  = &arg_get_gid,
	.set_func  = &arg_set_gid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GID,
	.help_short = "group_id",
	.help_long = "group ID to run job as (user root only)",
};

static struct slurm_long_option opt_sbatch_help = {
	.opt_group = OPT_GRP_HELP,
	.name      = "help",
	.get_func  = NULL,
	.set_func  = &arg_help,
	.exit_on_error = true,
	.pass      = 0,
	.has_arg   = no_argument,
	.opt_val   = 'h',
	.help_long = "show this help message",
};

static struct slurm_long_option opt_sbatch_hint = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "hint",
	.get_func  = &arg_get_hint,
	.set_func  = &arg_set_hint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_HINT,
	.help_long = "Bind tasks according to application hints (see \"--hint=help\" for options)",
};

static struct slurm_long_option opt_sbatch_ignore_pbs = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ignore-pbs",
	.get_func  = &arg_get_ignore_pbs,
	.set_func  = &arg_set_ignore_pbs,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_IGNORE_PBS,
	.help_long = "Ignore #PBS options in the batch script",
};

static struct slurm_long_option opt_sbatch_immediate = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "immediate",
	.get_func  = &arg_get_immediate,
	.set_func  = &arg_set_immediate,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'I',
};

static struct slurm_long_option opt_sbatch_input = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "input",
	.get_func  = &arg_get_input,
	.set_func  = &arg_set_input,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'i',
	.help_short = "in",
	.help_long = "file for batch script's standard input",
};

static struct slurm_long_option opt_sbatch_jobid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "jobid",
	.get_func  = &arg_get_jobid,
	.set_func  = &arg_set_jobid,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_JOBID,
	.help_short = "id",
	.help_long = "run under already allocated job",
};

static struct slurm_long_option opt_sbatch_kill_on_invalid_dep = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "kill-on-invalid-dep",
	.get_func  = &arg_get_kill_on_invalid_dep,
	.set_func  = &arg_set_kill_on_invalid_dep,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_KILL_INV_DEP,
};

static struct slurm_long_option opt_sbatch_mem_bind = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "mem_bind",
	.get_func  = &arg_get_mem_bind,
	.set_func  = &arg_set_mem_bind,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_BIND,
};

static struct slurm_long_option opt_sbatch_mem_per_cpu = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "mem-per-cpu",
	.get_func  = &arg_get_mem_per_cpu,
	.set_func  = &arg_set_mem_per_cpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_CPU,
	.help_short = "MB",
	.help_long = "maximum amount of real memory per allocated cpu required by the job.  --mem >= --mem-per-cpu if --mem is specified.",
};

static struct slurm_long_option opt_sbatch_mem_per_gpu = {
	.opt_group = OPT_GRP_GPU,
	.name      = "mem-per-gpu",
	.get_func  = &arg_get_mem_per_gpu,
	.set_func  = &arg_set_mem_per_gpu,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_GPU,
	.help_short = "n",
	.help_long = "real memory required per allocated GPU",
};

static struct slurm_long_option opt_sbatch_mincpus = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "mincpus",
	.get_func  = &arg_get_mincpus,
	.set_func  = &arg_set_mincpus,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINCPU,
	.help_short = "n",
	.help_long = "minimum number of logical processors (threads) per node",
};

static struct slurm_long_option opt_sbatch_network = {
	.opt_group = OPT_GRP_CRAY,
	.name      = "network",
	.get_func  = &arg_get_network,
	.set_func  = &arg_set_network,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NETWORK,
	.help_short = "type",
	.help_long = "Use network performance counters (system, network, or processor)",
};

static struct slurm_long_option opt_sbatch_no_requeue = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "no-requeue",
	.get_func  = &arg_get_no_requeue,
	.set_func  = &arg_set_no_requeue,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_NO_REQUEUE,
	.help_long = "if set, do not permit the job to be requeued",
};

static struct slurm_long_option opt_sbatch_nodefile = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "nodefile",
	.get_func  = &arg_get_nodefile,
	.set_func  = &arg_set_nodefile,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'F',
	.help_short = "filename",
	.help_long = "request a specific list of hosts",
};

static struct slurm_long_option opt_sbatch_ntasks = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks",
	.get_func  = &arg_get_ntasks,
	.set_func  = &arg_set_ntasks,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'n',
	.help_short = "ntasks",
	.help_long = "number of tasks to run",
};

static struct slurm_long_option opt_sbatch_ntasks_per_node = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks-per-node",
	.get_func  = &arg_get_ntasks_per_node,
	.set_func  = &arg_set_ntasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
	.help_short = "n",
	.help_long = "number of tasks to invoke on each node",
};

static struct slurm_long_option opt_sbatch_open_mode = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "open-mode",
	.get_func  = &arg_get_open_mode,
	.set_func  = &arg_set_open_mode,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_OPEN_MODE,
};

static struct slurm_long_option opt_sbatch_output = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "output",
	.get_func  = &arg_get_output,
	.set_func  = &arg_set_output,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'o',
	.help_short = "out",
	.help_long = "file for batch script's standard output",
};

static struct slurm_long_option opt_sbatch_oversubscribe = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "oversubscribe",
	.get_func  = &arg_get_share,
	.set_func  = &arg_set_share,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 's',
	.help_long = "over subscribe resources with other jobs",
};

static struct slurm_long_option opt_sbatch_parsable = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "parsable",
	.get_func  = &arg_get_parsable,
	.set_func  = &arg_set_parsable,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_PARSABLE,
	.help_long = "outputs only the jobid and cluster name (if present), separated by semicolon, only on successful submission.",
};

static struct slurm_long_option opt_sbatch_propagate = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "propagate",
	.get_func  = &arg_get_propagate,
	.set_func  = &arg_set_propagate,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_PROPAGATE,
	.help_short = "rlimits",
	.help_long = "propagate all [or specific list of] rlimits",
};

static struct slurm_long_option opt_sbatch_quiet = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "quiet",
	.get_func  = &arg_get_quiet,
	.set_func  = &arg_set_quiet,
	.exit_on_error = true,
	.pass      = 0,
	.has_arg   = no_argument,
	.opt_val   = 'Q',
	.help_long = "quiet mode (suppress informational messages)",
};

static struct slurm_long_option opt_sbatch_reboot = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "reboot",
	.get_func  = &arg_get_reboot,
	.set_func  = &arg_set_reboot,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_REBOOT,
	.help_long = "reboot compute nodes before starting job",
};

static struct slurm_long_option opt_sbatch_requeue = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "requeue",
	.get_func  = &arg_get_requeue,
	.set_func  = &arg_set_requeue,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_REQUEUE,
	.help_long = "if set, permit the job to be requeued",
};

static struct slurm_long_option opt_sbatch_sockets_per_node = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "sockets-per-node",
	.get_func  = &arg_get_sockets_per_node,
	.set_func  = &arg_set_sockets_per_node,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_SOCKETSPERNODE,
	.help_short = "S",
	.help_long = "number of sockets per node to allocate",
};

static struct slurm_long_option opt_sbatch_switches = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "switches",
	.get_func  = &arg_get_switches,
	.set_func  = &arg_set_switches,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_REQ_SWITCH,
	.help_short = "max-switches{@max-time-to-wait}",
	.help_long = "Optimum switches and max time to wait for optimum thread-spec=threads   count of reserved threads",
};

static struct slurm_long_option opt_sbatch_tasks = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tasks",
	.get_func  = &arg_get_ntasks,
	.set_func  = &arg_set_ntasks,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'n',
};

static struct slurm_long_option opt_sbatch_tasks_per_node = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tasks-per-node",
	.get_func  = &arg_get_ntasks_per_node,
	.set_func  = &arg_set_ntasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
};

static struct slurm_long_option opt_sbatch_test_only = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "test-only",
	.get_func  = &arg_get_test_only,
	.set_func  = &arg_set_test_only,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_TEST_ONLY,
};

static struct slurm_long_option opt_sbatch_thread_spec = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "thread-spec",
	.get_func  = &arg_get_thread_spec,
	.set_func  = &arg_set_thread_spec,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREAD_SPEC,
};

static struct slurm_long_option opt_sbatch_threads_per_core = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "threads-per-core",
	.get_func  = &arg_get_threads_per_core,
	.set_func  = &arg_set_threads_per_core,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREADSPERCORE,
	.help_short = "T",
	.help_long = "number of threads per core to allocate",
};

static struct slurm_long_option opt_sbatch_uid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "uid",
	.get_func  = &arg_get_uid,
	.set_func  = &arg_set_uid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_UID,
	.help_short = "user_id",
	.help_long = "user ID to run job as (user root only)",
};

static struct slurm_long_option opt_sbatch_usage = {
	.opt_group = OPT_GRP_HELP,
	.name      = "usage",
	.get_func  = NULL,
	.set_func  = &arg_usage,
	.exit_on_error = true,
	.pass      = 0,
	.has_arg   = no_argument,
	.opt_val   = 'u',
	.help_long = "display brief usage message",
};

static struct slurm_long_option opt_sbatch_verbose = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "verbose",
	.get_func  = &arg_get_verbose,
	.set_func  = &arg_set_verbose,
	.pass      = 0,
	.has_arg   = no_argument,
	.opt_val   = 'v',
	.help_long = "verbose mode (multiple -v's increase verbosity)",
};

static struct slurm_long_option opt_sbatch_version = {
	.opt_group = OPT_GRP_OTHER,
	.name      = "version",
	.get_func  = NULL,
	.set_func  = &arg_version,
	.exit_on_error = true,
	.pass      = 0,
	.has_arg   = no_argument,
	.opt_val   = 'V',
	.help_long = "output version information and exit",
};

static struct slurm_long_option opt_sbatch_wait = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "wait",
	.get_func  = &arg_get_wait,
	.set_func  = &arg_set_wait,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'W',
	.help_long = "wait for completion of submitted job",
};

static struct slurm_long_option opt_sbatch_wait_all_nodes = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "wait-all-nodes",
	.get_func  = &arg_get_wait_all_nodes,
	.set_func  = &arg_set_wait_all_nodes,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_WAIT_ALL_NODES,
};

static struct slurm_long_option opt_sbatch_workdir = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "workdir",
	.get_func  = &arg_get_workdir,
	.set_func  = &arg_set_workdir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'D',
};

static struct slurm_long_option opt_sbatch_wrap = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "wrap",
	.get_func  = &arg_get_wrap,
	.set_func  = &arg_set_wrap,
	.pass      = 0,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_WRAP,
	.help_short = "command",
	.help_long = "string] wrap command string in a sh script and submit",
};

static struct slurm_long_option opt_srun_accel_bind = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "accel-bind",
	.get_func  = &arg_get_accel_bind,
	.set_func  = &arg_set_accel_bind,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_ACCEL_BIND,
};

static struct slurm_long_option opt_srun_acctg_freq = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "acctg-freq",
	.get_func  = &arg_get_acctg_freq,
	.set_func  = &arg_set_acctg_freq,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_ACCTG_FREQ,
	.help_short = "<datatype>=<interval>",
	.help_long = "accounting and profiling sampling intervals. Supported datatypes: task=<interval> energy=<interval> network=<interval> filesystem=<interval>",
};

static struct slurm_long_option opt_srun_bcast = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "bcast",
	.get_func  = &arg_get_bcast,
	.set_func  = &arg_set_bcast,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_BCAST,
	.help_short = "<dest_path>",
	.help_long = "Copy executable file to compute nodes",
};

static struct slurm_long_option opt_srun_chdir = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "chdir",
	.get_func  = &arg_get_chdir,
	.set_func  = &arg_set_chdir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'D',
	.help_short = "path",
	.help_long = "change remote current working directory",
};

static struct slurm_long_option opt_srun_checkpoint = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "checkpoint",
	.get_func  = &arg_get_checkpoint,
	.set_func  = &arg_set_checkpoint,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CHECKPOINT,
	.help_short = "time",
	.help_long = "job step checkpoint interval",
};

static struct slurm_long_option opt_srun_checkpoint_dir = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "checkpoint-dir",
	.get_func  = &arg_get_checkpoint_dir,
	.set_func  = &arg_set_checkpoint_dir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CHECKPOINT_DIR,
	.help_short = "dir",
	.help_long = "directory to store job step checkpoint image files",
};

static struct slurm_long_option opt_srun_cluster = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "cluster",
	.get_func  = &arg_get_clusters,
	.set_func  = &arg_set_clusters,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
};

static struct slurm_long_option opt_srun_cluster_constraint = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "cluster-constraint",
	.get_func  = &arg_get_cluster_constraint,
	.set_func  = &arg_set_cluster_constraint,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CLUSTER_CONSTRAINT,
	.help_short = "list",
	.help_long = "specify a list of cluster-constraints",
};

static struct slurm_long_option opt_srun_clusters = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "clusters",
	.get_func  = &arg_get_clusters,
	.set_func  = &arg_set_clusters,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'M',
	.help_short = "names",
	.help_long = "Comma separated list of clusters to issue commands to. Default is current cluster. Name of 'all' will submit to run on all clusters. NOTE: SlurmDBD must up.",
};

static struct slurm_long_option opt_srun_compress = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "compress",
	.get_func  = &arg_get_compress,
	.set_func  = &arg_set_compress,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_COMPRESS,
	.help_short = "library",
	.help_long = "data compression library used with --bcast",
};

static struct slurm_long_option opt_srun_cores_per_socket = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "cores-per-socket",
	.get_func  = &arg_get_cores_per_socket,
	.set_func  = &arg_set_cores_per_socket,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CORESPERSOCKET,
	.help_short = "C",
	.help_long = "number of cores per socket to allocate",
};

static struct slurm_long_option opt_srun_cpu_bind = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "cpu_bind",
	.get_func  = &arg_get_cpu_bind,
	.set_func  = &arg_set_cpu_bind,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_CPU_BIND,
};

static struct slurm_long_option opt_srun_debugger_test = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "debugger-test",
	.get_func  = &arg_get_debugger_test,
	.set_func  = &arg_set_debugger_test,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_DEBUG_TS,
};

static struct slurm_long_option opt_srun_disable_status = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "disable-status",
	.get_func  = &arg_get_disable_status,
	.set_func  = &arg_set_disable_status,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'X',
	.help_long = "Disable Ctrl-C status feature",
};

static struct slurm_long_option opt_srun_epilog = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "epilog",
	.get_func  = &arg_get_epilog,
	.set_func  = &arg_set_epilog,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_EPILOG,
	.help_short = "program",
	.help_long = "run \"program\" after launching job step",
};

static struct slurm_long_option opt_srun_error = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "error",
	.get_func  = &arg_get_error,
	.set_func  = &arg_set_error,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'e',
	.help_short = "err",
	.help_long = "location of stderr redirection",
};

static struct slurm_long_option opt_srun_exclusive = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "exclusive",
	.get_func  = &arg_get_exclusive,
	.set_func  = &arg_set_exclusive,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_EXCLUSIVE,
	.help_short = "user|mcs",
	.help_long = "Job Steps: don't share CPUs; Job Allocations: allocate nodes in exclusive mode when cpu consumable resource is enabled. Optional flag \"user\" ensures nodes only share jobs from the same user. Optional flag \"mcs\" ensures nodes only share jobs from the same MCS group.",
};

static struct slurm_long_option opt_srun_export = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "export",
	.get_func  = &arg_get_export,
	.set_func  = &arg_set_export,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_EXPORT,
	.help_short = "env_vars|NONE",
	.help_long = "environment variables passed to launcher with optional values or NONE (pass no variables)",
};

static struct slurm_long_option opt_srun_extra_node_info = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "extra-node-info",
	.get_func  = &arg_get_extra_node_info,
	.set_func  = &arg_set_extra_node_info,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'B',
	.help_short = "S[:C[:T]]",
	.help_long = "Expands to: S=sockets-per-node, C=cores-per-socket, T=threads-per-core. Each field can be 'min' or wildcard '*'; total cpus requested = (N x S x C x T)",
};

static struct slurm_long_option opt_srun_get_user_env = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "get-user-env",
	.get_func  = &arg_get_get_user_env,
	.set_func  = &arg_set_get_user_env,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_GET_USER_ENV,
	.help_long = "used by Moab.  See srun man page.",
};

static struct slurm_long_option opt_srun_gid = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "gid",
	.get_func  = &arg_get_gid,
	.set_func  = &arg_set_gid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_GID,
};

static struct slurm_long_option opt_srun_help = {
	.opt_group = OPT_GRP_HELP,
	.name      = "help",
	.get_func  = NULL,
	.set_func  = &arg_help,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'h',
	.help_long = "show this help message",
};

static struct slurm_long_option opt_srun_hint = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "hint",
	.get_func  = &arg_get_hint,
	.set_func  = &arg_set_hint,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_HINT,
	.help_long = "Bind tasks according to application hints (see \"--hint=help\" for options)",
};

static struct slurm_long_option opt_srun_immediate = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "immediate",
	.get_func  = &arg_get_immediate,
	.set_func  = &arg_set_immediate,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = 'I',
	.help_short = "secs",
	.help_long = "exit if resources not available in \"secs\"",
};

static struct slurm_long_option opt_srun_input = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "input",
	.get_func  = &arg_get_input,
	.set_func  = &arg_set_input,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'i',
	.help_short = "in",
	.help_long = "location of stdin redirection",
};

static struct slurm_long_option opt_srun_jobid = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "jobid",
	.get_func  = &arg_get_jobid,
	.set_func  = &arg_set_jobid,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_JOBID,
	.help_short = "id",
	.help_long = "run under already allocated job",
};

static struct slurm_long_option opt_srun_join = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "join",
	.get_func  = &arg_get_join,
	.set_func  = &arg_set_join,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'j',
};

static struct slurm_long_option opt_srun_kill_on_bad_exit = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "kill-on-bad-exit",
	.get_func  = &arg_get_kill_on_bad_exit,
	.set_func  = &arg_set_kill_on_bad_exit,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = 'K',
	.help_long = "kill the job if any task terminates with a non-zero exit code",
};

static struct slurm_long_option opt_srun_label = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "label",
	.get_func  = &arg_get_label,
	.set_func  = &arg_set_label,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'l',
	.help_long = "prepend task number to lines of stdout/err",
};

static struct slurm_long_option opt_srun_launch_cmd = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "launch-cmd",
	.get_func  = &arg_get_launch_cmd,
	.set_func  = &arg_set_launch_cmd,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_LAUNCH_CMD,
	.help_long = "print external launcher command line if not Slurm",
};

static struct slurm_long_option opt_srun_launcher_opts = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "launcher-opts",
	.get_func  = &arg_get_launcher_opts,
	.set_func  = &arg_set_launcher_opts,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_LAUNCHER_OPTS,
	.help_long = "options for the external launcher command if not Slurm",
};

static struct slurm_long_option opt_srun_mem_bind = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "mem_bind",
	.get_func  = &arg_get_mem_bind,
	.set_func  = &arg_set_mem_bind,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_BIND,
};

static struct slurm_long_option opt_srun_mem_per_cpu = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "mem-per-cpu",
	.get_func  = &arg_get_mem_per_cpu,
	.set_func  = &arg_set_mem_per_cpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_CPU,
	.help_short = "MB",
	.help_long = "maximum amount of real memory per allocated cpu required by the job. --mem >= --mem-per-cpu if --mem is specified.",
};

static struct slurm_long_option opt_srun_mem_per_gpu = {
	.opt_group = OPT_GRP_GPU,
	.name      = "mem-per-gpu",
	.get_func  = &arg_get_mem_per_gpu,
	.set_func  = &arg_set_mem_per_gpu,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MEM_PER_GPU,
	.help_short = "n",
	.help_long = "real memory required per allocated GPU",
};

static struct slurm_long_option opt_srun_mincpus = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "mincpus",
	.get_func  = &arg_get_mincpus,
	.set_func  = &arg_set_mincpus,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MINCPUS,
	.help_short = "n",
	.help_long = "minimum number of logical processors (threads) per node",
};

static struct slurm_long_option opt_srun_mpi = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "mpi",
	.get_func  = &arg_get_mpi,
	.set_func  = &arg_set_mpi,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_MPI,
	.help_short = "type",
	.help_long = "type of MPI being used",
};

static struct slurm_long_option opt_srun_msg_timeout = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "msg-timeout",
	.get_func  = &arg_get_msg_timeout,
	.set_func  = &arg_set_msg_timeout,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TIMEO,
};

static struct slurm_long_option opt_srun_multi_prog = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "multi-prog",
	.get_func  = &arg_get_multi_prog,
	.set_func  = &arg_set_multi_prog,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_MULTI,
	.help_long = "if set the program name specified is the configuration specification for multiple programs",
};

static struct slurm_long_option opt_srun_network = {
	.name      = "network",
	.get_func  = &arg_get_network,
	.set_func  = &arg_set_network,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NETWORK,
	.help_short = "type",
#if defined HAVE_NATIVE_CRAY
	.help_long = "Use network performance counters (system, network, or processor)",
	.opt_group = OPT_GRP_CRAY,
#else
	.opt_group = OPT_GRP_UNKNOWN,
#endif
};

static struct slurm_long_option opt_srun_no_allocate = {
	.opt_group = OPT_GRP_CONSTRAINT,
	.name      = "no-allocate",
	.get_func  = &arg_get_no_allocate,
	.set_func  = &arg_set_no_allocate,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'Z',
	.help_long = "don't allocate nodes (must supply -w)",
};

static struct slurm_long_option opt_srun_ntasks = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks",
	.get_func  = &arg_get_ntasks,
	.set_func  = &arg_set_ntasks,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'n',
	.help_short = "ntasks",
	.help_long = "number of tasks to run",
};

static struct slurm_long_option opt_srun_ntasks_per_node = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "ntasks-per-node",
	.get_func  = &arg_get_ntasks_per_node,
	.set_func  = &arg_set_ntasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
	.help_short = "n",
	.help_long = "number of tasks to invoke on each node",
};

static struct slurm_long_option opt_srun_open_mode = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "open-mode",
	.get_func  = &arg_get_open_mode,
	.set_func  = &arg_set_open_mode,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_OPEN_MODE,
};

static struct slurm_long_option opt_srun_output = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "output",
	.get_func  = &arg_get_output,
	.set_func  = &arg_set_output,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'o',
	.help_short = "out",
	.help_long = "location of stdout redirection",
};

static struct slurm_long_option opt_srun_oversubscribe = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "oversubscribe",
	.get_func  = &arg_get_share,
	.set_func  = &arg_set_share,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 's',
	.help_long = "over-subscribe resources with other jobs",
};

static struct slurm_long_option opt_srun_pack_group = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "pack-group",
	.get_func  = &arg_get_pack_group,
	.set_func  = &arg_set_pack_group,
	.pass      = 0,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_PACK_GROUP,
	.help_short = "value",
	.help_long = "pack job allocation(s) in which to launch application",
};

static struct slurm_long_option opt_srun_preserve_env = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "preserve-env",
	.get_func  = &arg_get_preserve_env,
	.set_func  = &arg_set_preserve_env,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'E',
	.help_long = "env vars for node and task counts override command-line flags",
};

static struct slurm_long_option opt_srun_preserve_slurm_env = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "preserve-slurm-env",
	.get_func  = &arg_get_preserve_env,
	.set_func  = &arg_set_preserve_env,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'E',
};

static struct slurm_long_option opt_srun_prolog = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "prolog",
	.get_func  = &arg_get_prolog,
	.set_func  = &arg_set_prolog,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_PROLOG,
	.help_short = "program",
	.help_long = "run \"program\" before launching job step",
};

static struct slurm_long_option opt_srun_propagate = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "propagate",
	.get_func  = &arg_get_propagate,
	.set_func  = &arg_set_propagate,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_PROPAGATE,
	.help_short = "rlimits",
	.help_long = "propagate all [or specific list of] rlimits",
};

static struct slurm_long_option opt_srun_pty = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "pty",
	.get_func  = &arg_get_pty,
	.set_func  = &arg_set_pty,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_PTY,
	.help_long = "run task zero in pseudo terminal",
};

static struct slurm_long_option opt_srun_quiet = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "quiet",
	.get_func  = &arg_get_quiet,
	.set_func  = &arg_set_quiet,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'Q',
	.help_long = "quiet mode (suppress informational messages)",
};

static struct slurm_long_option opt_srun_quit_on_interrupt = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "quit-on-interrupt",
	.get_func  = &arg_get_quit_on_interrupt,
	.set_func  = &arg_set_quit_on_interrupt,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_QUIT_ON_INTR,
	.help_long = "quit on single Ctrl-C",
};

static struct slurm_long_option opt_srun_reboot = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "reboot",
	.get_func  = &arg_get_reboot,
	.set_func  = &arg_set_reboot,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_REBOOT,
	.help_long = "reboot block before starting job",
};

static struct slurm_long_option opt_srun_relative = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "relative",
	.get_func  = &arg_get_relative,
	.set_func  = &arg_set_relative,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'r',
	.help_short = "n",
	.help_long = "run job step relative to node n of allocation",
};

static struct slurm_long_option opt_srun_restart_dir = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "restart-dir",
	.get_func  = &arg_get_restart_dir,
	.set_func  = &arg_set_restart_dir,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_RESTART_DIR,
	.help_short = "dir",
	.help_long = "directory of checkpoint image files to restart from",
};

static struct slurm_long_option opt_srun_resv_ports = {
	.opt_group = OPT_GRP_CONSRES,
	.name      = "resv-ports",
	.get_func  = &arg_get_resv_ports,
	.set_func  = &arg_set_resv_ports,
	.pass      = 1,
	.has_arg   = optional_argument,
	.opt_val   = LONG_OPT_RESV_PORTS,
	.help_long = "reserve communication ports",
};

static struct slurm_long_option opt_srun_slurmd_debug = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "slurmd-debug",
	.get_func  = &arg_get_slurmd_debug,
	.set_func  = &arg_set_slurmd_debug,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_DEBUG_SLURMD,
	.help_short = "level",
	.help_long = "slurmd debug level",
};

static struct slurm_long_option opt_srun_sockets_per_node = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "sockets-per-node",
	.get_func  = &arg_get_sockets_per_node,
	.set_func  = &arg_set_sockets_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_SOCKETSPERNODE,
	.help_short = "S",
	.help_long = "number of sockets per node to allocate",
};

static struct slurm_long_option opt_srun_switches = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "switches",
	.get_func  = &arg_get_switches,
	.set_func  = &arg_set_switches,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_REQ_SWITCH,
	.help_short = "max-switches{@max-time-to-wait}",
	.help_long = "Optimum switches and max time to wait for optimum",
};

static struct slurm_long_option opt_srun_task_epilog = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "task-epilog",
	.get_func  = &arg_get_task_epilog,
	.set_func  = &arg_set_task_epilog,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TASK_EPILOG,
	.help_short = "program",
	.help_long = "run \"program\" after launching task",
};

static struct slurm_long_option opt_srun_task_prolog = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "task-prolog",
	.get_func  = &arg_get_task_prolog,
	.set_func  = &arg_set_task_prolog,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TASK_PROLOG,
	.help_short = "program",
	.help_long = "run \"program\" before launching task",
};

static struct slurm_long_option opt_srun_tasks_per_node = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tasks-per-node",
	.get_func  = &arg_get_ntasks_per_node,
	.set_func  = &arg_set_ntasks_per_node,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_NTASKSPERNODE,
};

static struct slurm_long_option opt_srun_test_only = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "test-only",
	.get_func  = &arg_get_test_only,
	.set_func  = &arg_set_test_only,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_TEST_ONLY,
};

static struct slurm_long_option opt_srun_thread_spec = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "thread-spec",
	.get_func  = &arg_get_thread_spec,
	.set_func  = &arg_set_thread_spec,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREAD_SPEC,
	.help_short = "threads",
	.help_long = "count of reserved threads",
};

static struct slurm_long_option opt_srun_threads = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "threads",
	.get_func  = &arg_get_threads,
	.set_func  = &arg_set_threads,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'T',
	.help_short = "threads",
	.help_long = "set srun launch fanout",
};

static struct slurm_long_option opt_srun_threads_per_core = {
	.opt_group = OPT_GRP_AFFINITY,
	.name      = "threads-per-core",
	.get_func  = &arg_get_threads_per_core,
	.set_func  = &arg_set_threads_per_core,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_THREADSPERCORE,
	.help_short = "T",
	.help_long = "number of threads per core to allocate",
};

static struct slurm_long_option opt_srun_tres_per_job = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "tres-per-job",
	.get_func  = &arg_get_tres_per_job,
	.set_func  = &arg_set_tres_per_job,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_TRES_PER_JOB,
};

static struct slurm_long_option opt_srun_uid = {
	.opt_group = OPT_GRP_UNKNOWN,
	.name      = "uid",
	.get_func  = &arg_get_uid,
	.set_func  = &arg_set_uid,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = LONG_OPT_UID,
};

static struct slurm_long_option opt_srun_unbuffered = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "unbuffered",
	.get_func  = &arg_get_unbuffered,
	.set_func  = &arg_set_unbuffered,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'u',
	.help_long = "do not line-buffer stdout/err",
};

static struct slurm_long_option opt_srun_usage = {
	.opt_group = OPT_GRP_HELP,
	.name      = "usage",
	.get_func  = NULL,
	.set_func  = &arg_usage,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = LONG_OPT_USAGE,
	.help_long = "display brief usage message",
};

static struct slurm_long_option opt_srun_verbose = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "verbose",
	.get_func  = &arg_get_verbose,
	.set_func  = &arg_set_verbose,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'v',
	.help_long = "verbose mode (multiple -v's increase verbosity)",
};

static struct slurm_long_option opt_srun_version = {
	.opt_group = OPT_GRP_OTHER,
	.name      = "version",
	.get_func  = NULL,
	.set_func  = &arg_version,
	.exit_on_error = true,
	.pass      = 1,
	.has_arg   = no_argument,
	.opt_val   = 'V',
	.help_long = "output version information and exit",
};

static struct slurm_long_option opt_srun_wait = {
	.opt_group = OPT_GRP_PARRUN,
	.name      = "wait",
	.get_func  = &arg_get_wait,
	.set_func  = &arg_set_wait,
	.pass      = 1,
	.has_arg   = required_argument,
	.opt_val   = 'W',
	.help_short = "sec",
	.help_long = "seconds to wait after first task exits before killing job",
};

struct slurm_long_option *srun_options[] = {
	&opt_common_account,
	&opt_common_account_deprecated,
	&opt_common_bb,
	&opt_common_bbf,
	&opt_common_begin,
	&opt_common_comment,
	&opt_common_constraint,
	&opt_common_contiguous,
	&opt_common_core_spec,
	&opt_common_cpu_freq,
	&opt_common_cpus_per_gpu,
	&opt_common_cpus_per_task,
	&opt_common_deadline,
	&opt_common_delay_boot,
	&opt_common_dependency,
	&opt_common_dependency_deprecated,
	&opt_common_distribution,
	&opt_common_exclude,
	&opt_common_gpu_bind,
	&opt_common_gpu_freq,
	&opt_common_gpus,
	&opt_common_gpus_per_node,
	&opt_common_gpus_per_socket,
	&opt_common_gpus_per_task,
	&opt_common_gres,
	&opt_common_gres_flags,
	&opt_common_hold,
	&opt_common_job_name,
	&opt_common_licenses,
	&opt_common_mail_type,
	&opt_common_mail_user,
	&opt_common_mcs_label,
	&opt_common_mem,
	&opt_common_mincores,
	&opt_common_minsockets,
	&opt_common_minthreads,
	&opt_common_nice,
	&opt_common_no_kill,
	&opt_common_nodelist,
	&opt_common_nodes,
	&opt_common_ntasks_per_core,
	&opt_common_ntasks_per_socket,
	&opt_common_overcommit,
	&opt_common_partition,
	&opt_common_power,
	&opt_common_priority,
	&opt_common_profile,
	&opt_common_qos,
	&opt_common_reservation,
	&opt_common_share,
	&opt_common_signal,
	&opt_common_spread_job,
	&opt_common_time,
	&opt_common_time_min,
	&opt_common_tmp,
	&opt_common_use_min_nodes,
	&opt_common_wckey,
	&opt_common_x11,
	&opt_srun_accel_bind,
	&opt_srun_acctg_freq,
	&opt_srun_bcast,
	&opt_srun_chdir,
	&opt_srun_checkpoint,
	&opt_srun_checkpoint_dir,
	&opt_srun_cluster,
	&opt_srun_cluster_constraint,
	&opt_srun_clusters,
	&opt_srun_compress,
	&opt_srun_cores_per_socket,
	&opt_srun_cpu_bind,
	&opt_srun_cpu_bind,
	&opt_srun_debugger_test,
	&opt_srun_disable_status,
	&opt_srun_epilog,
	&opt_srun_error,
	&opt_srun_exclusive,
	&opt_srun_export,
	&opt_srun_extra_node_info,
	&opt_srun_get_user_env,
	&opt_srun_gid,
	&opt_srun_help,
	&opt_srun_hint,
	&opt_srun_immediate,
	&opt_srun_input,
	&opt_srun_jobid,
	&opt_srun_join,
	&opt_srun_kill_on_bad_exit,
	&opt_srun_label,
	&opt_srun_launch_cmd,
	&opt_srun_launcher_opts,
	&opt_srun_mem_bind,
	&opt_srun_mem_bind,
	&opt_srun_mem_per_cpu,
	&opt_srun_mem_per_gpu,
	&opt_srun_mincpus,
	&opt_srun_mpi,
	&opt_srun_msg_timeout,
	&opt_srun_multi_prog,
	&opt_srun_network,
	&opt_srun_no_allocate,
	&opt_srun_ntasks,
	&opt_srun_ntasks_per_node,
	&opt_srun_open_mode,
	&opt_srun_output,
	&opt_srun_oversubscribe,
	&opt_srun_pack_group,
	&opt_srun_preserve_env,
	&opt_srun_preserve_slurm_env,
	&opt_srun_prolog,
	&opt_srun_propagate,
	&opt_srun_pty,
	&opt_srun_quiet,
	&opt_srun_quit_on_interrupt,
	&opt_srun_reboot,
	&opt_srun_relative,
	&opt_srun_restart_dir,
	&opt_srun_resv_ports,
	&opt_srun_slurmd_debug,
	&opt_srun_sockets_per_node,
	&opt_srun_switches,
	&opt_srun_task_epilog,
	&opt_srun_task_prolog,
	&opt_srun_tasks_per_node,
	&opt_srun_test_only,
	&opt_srun_thread_spec,
	&opt_srun_threads,
	&opt_srun_threads_per_core,
	&opt_srun_tres_per_job,
	&opt_srun_uid,
	&opt_srun_unbuffered,
	&opt_srun_usage,
	&opt_srun_verbose,
	&opt_srun_version,
	&opt_srun_wait,
	NULL
};
struct slurm_long_option *salloc_options[] = {
	&opt_common_account,
	&opt_common_account_deprecated,
	&opt_common_bb,
	&opt_common_bbf,
	&opt_common_begin,
	&opt_common_comment,
	&opt_common_constraint,
	&opt_common_contiguous,
	&opt_common_core_spec,
	&opt_common_cpu_freq,
	&opt_common_cpus_per_gpu,
	&opt_common_cpus_per_task,
	&opt_common_deadline,
	&opt_common_delay_boot,
	&opt_common_dependency,
	&opt_common_dependency_deprecated,
	&opt_common_distribution,
	&opt_common_exclude,
	&opt_common_gpu_bind,
	&opt_common_gpu_freq,
	&opt_common_gpus,
	&opt_common_gpus_per_node,
	&opt_common_gpus_per_socket,
	&opt_common_gpus_per_task,
	&opt_common_gres,
	&opt_common_gres_flags,
	&opt_common_hold,
	&opt_common_job_name,
	&opt_common_licenses,
	&opt_common_mail_type,
	&opt_common_mail_user,
	&opt_common_mcs_label,
	&opt_common_mem,
	&opt_common_mincores,
	&opt_common_minsockets,
	&opt_common_minthreads,
	&opt_common_nice,
	&opt_common_no_kill,
	&opt_common_nodelist,
	&opt_common_nodes,
	&opt_common_ntasks_per_core,
	&opt_common_ntasks_per_socket,
	&opt_common_overcommit,
	&opt_common_partition,
	&opt_common_power,
	&opt_common_priority,
	&opt_common_profile,
	&opt_common_qos,
	&opt_common_reservation,
	&opt_common_share,
	&opt_common_signal,
	&opt_common_spread_job,
	&opt_common_time,
	&opt_common_time_min,
	&opt_common_tmp,
	&opt_common_use_min_nodes,
	&opt_common_wckey,
	&opt_common_x11,
	&opt_salloc_acctg_freq,
	&opt_salloc_bell,
	&opt_salloc_chdir,
	&opt_salloc_cluster,
	&opt_salloc_cluster_constraint,
	&opt_salloc_clusters,
	&opt_salloc_cores_per_socket,
	&opt_salloc_exclusive,
	&opt_salloc_extra_node_info,
	&opt_salloc_get_user_env,
	&opt_salloc_gid,
	&opt_salloc_help,
	&opt_salloc_hint,
	&opt_salloc_immediate,
	&opt_salloc_jobid,
	&opt_salloc_kill_command,
	&opt_salloc_mem_bind,
	&opt_salloc_mem_bind,
	&opt_salloc_mem_per_cpu,
	&opt_salloc_mem_per_gpu,
	&opt_salloc_mincpus,
	&opt_salloc_network,
	&opt_salloc_no_bell,
	&opt_salloc_no_shell,
	&opt_salloc_nodefile,
	&opt_salloc_ntasks,
	&opt_salloc_ntasks_per_node,
	&opt_salloc_oversubscribe,
	&opt_salloc_quiet,
	&opt_salloc_reboot,
	&opt_salloc_sockets_per_node,
	&opt_salloc_switches,
	&opt_salloc_tasks,
	&opt_salloc_tasks_per_node,
	&opt_salloc_thread_spec,
	&opt_salloc_threads_per_core,
	&opt_salloc_uid,
	&opt_salloc_usage,
	&opt_salloc_verbose,
	&opt_salloc_version,
	&opt_salloc_wait,
	&opt_salloc_wait_all_nodes,
	NULL
};
struct slurm_long_option *sbatch_options[] = {
	&opt_common_account,
	&opt_common_account_deprecated,
	&opt_common_bb,
	&opt_common_bbf,
	&opt_common_begin,
	&opt_common_comment,
	&opt_common_constraint,
	&opt_common_contiguous,
	&opt_common_core_spec,
	&opt_common_cpu_freq,
	&opt_common_cpus_per_gpu,
	&opt_common_cpus_per_task,
	&opt_common_deadline,
	&opt_common_delay_boot,
	&opt_common_dependency,
	&opt_common_dependency_deprecated,
	&opt_common_distribution,
	&opt_common_exclude,
	&opt_common_gpu_bind,
	&opt_common_gpu_freq,
	&opt_common_gpus,
	&opt_common_gpus_per_node,
	&opt_common_gpus_per_socket,
	&opt_common_gpus_per_task,
	&opt_common_gres,
	&opt_common_gres_flags,
	&opt_common_hold,
	&opt_common_job_name,
	&opt_common_licenses,
	&opt_common_mail_type,
	&opt_common_mail_user,
	&opt_common_mcs_label,
	&opt_common_mem,
	&opt_common_mincores,
	&opt_common_minsockets,
	&opt_common_minthreads,
	&opt_common_nice,
	&opt_common_no_kill,
	&opt_common_nodelist,
	&opt_common_nodes,
	&opt_common_ntasks_per_core,
	&opt_common_ntasks_per_socket,
	&opt_common_overcommit,
	&opt_common_partition,
	&opt_common_power,
	&opt_common_priority,
	&opt_common_profile,
	&opt_common_qos,
	&opt_common_reservation,
	&opt_common_share,
	&opt_common_signal,
	&opt_common_spread_job,
	&opt_common_time,
	&opt_common_time_min,
	&opt_common_tmp,
	&opt_common_use_min_nodes,
	&opt_common_wckey,
	&opt_common_x11,
	&opt_sbatch_acctg_freq,
	&opt_sbatch_array,
	&opt_sbatch_batch,
	&opt_sbatch_chdir,
	&opt_sbatch_checkpoint,
	&opt_sbatch_checkpoint_dir,
	&opt_sbatch_cluster,
	&opt_sbatch_cluster_constraint,
	&opt_sbatch_clusters,
	&opt_sbatch_cores_per_socket,
	&opt_sbatch_error,
	&opt_sbatch_exclusive,
	&opt_sbatch_export,
	&opt_sbatch_export_file,
	&opt_sbatch_extra_node_info,
	&opt_sbatch_get_user_env,
	&opt_sbatch_gid,
	&opt_sbatch_help,
	&opt_sbatch_hint,
	&opt_sbatch_ignore_pbs,
	&opt_sbatch_immediate,
	&opt_sbatch_input,
	&opt_sbatch_jobid,
	&opt_sbatch_kill_on_invalid_dep,
	&opt_sbatch_mem_bind,
	&opt_sbatch_mem_bind,
	&opt_sbatch_mem_per_cpu,
	&opt_sbatch_mem_per_gpu,
	&opt_sbatch_mincpus,
	&opt_sbatch_network,
	&opt_sbatch_no_requeue,
	&opt_sbatch_nodefile,
	&opt_sbatch_ntasks,
	&opt_sbatch_ntasks_per_node,
	&opt_sbatch_open_mode,
	&opt_sbatch_output,
	&opt_sbatch_oversubscribe,
	&opt_sbatch_parsable,
	&opt_sbatch_propagate,
	&opt_sbatch_quiet,
	&opt_sbatch_reboot,
	&opt_sbatch_requeue,
	&opt_sbatch_sockets_per_node,
	&opt_sbatch_switches,
	&opt_sbatch_tasks,
	&opt_sbatch_tasks_per_node,
	&opt_sbatch_test_only,
	&opt_sbatch_thread_spec,
	&opt_sbatch_threads_per_core,
	&opt_sbatch_uid,
	&opt_sbatch_usage,
	&opt_sbatch_verbose,
	&opt_sbatch_version,
	&opt_sbatch_wait,
	&opt_sbatch_wait_all_nodes,
	&opt_sbatch_workdir,
	&opt_sbatch_wrap,
	NULL
};
