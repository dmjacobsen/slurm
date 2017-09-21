/*****************************************************************************\
 *  cli_filter_common.h - Common infrastructure available to all cli_filter
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

#ifndef _CLI_FILTER_COMMON_H_
#define _CLI_FILTER_COMMON_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <src/salloc/salloc_opt.h>
#include <src/sbatch/sbatch_opt.h>
#include <src/srun/libsrun/srun_opt.h>
#include <src/bcast/file_bcast.h>

typedef struct _string_option {
	char *name;
	size_t offset;
        char * (*read)(void *, const struct _string_option *);
        bool (*write)(const char *, void *, const struct _string_option *);
} cli_string_option_t;

const cli_string_option_t *cli_si_find(const char *name,
                                       const cli_string_option_t *opt);

extern cli_string_option_t salloc_options_si[];
extern cli_string_option_t sbatch_options_si[];
extern cli_string_option_t srun_options_si[];
extern cli_string_option_t bcast_parameters_si[];

char *cli_si_get(const char *, void *, int client_type);
bool cli_si_set(const char *, const char *, void *, int client_type);

char *cli_gen_json(uint32_t jobid, void *, int client_type);

char *cli_si_get_string(void *, const cli_string_option_t *);
bool cli_si_set_string(const char *, void *, const cli_string_option_t *);
char *cli_si_get_stringarray(void *, const cli_string_option_t *);
char *cli_si_get_int(void *, const cli_string_option_t *);
bool cli_si_set_int(const char *, void *, const cli_string_option_t *);
char *cli_si_get_long(void *, const cli_string_option_t *);
bool cli_si_set_long(const char *, void *, const cli_string_option_t *);
char *cli_si_get_unsigned(void *, const cli_string_option_t *);
bool cli_si_set_unsigned(const char *, void *, const cli_string_option_t *);
char *cli_si_get_uid(void *, const cli_string_option_t *);
char *cli_si_get_gid(void *, const cli_string_option_t *);
char *cli_si_get_int8_t(void *, const cli_string_option_t *);
bool cli_si_set_int8_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_int16_t(void *, const cli_string_option_t *);
bool cli_si_set_int16_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_int32_t(void *, const cli_string_option_t *);
bool cli_si_set_int32_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_int64_t(void *, const cli_string_option_t *);
bool cli_si_set_int64_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_uint8_t(void *, const cli_string_option_t *);
bool cli_si_set_uint8_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_uint16_t(void *, const cli_string_option_t *);
bool cli_si_set_uint16_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_uint32_t(void *, const cli_string_option_t *);
bool cli_si_set_uint32_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_uint64_t(void *, const cli_string_option_t *);
bool cli_si_set_uint64_t(const char *, void *, const cli_string_option_t *);
char *cli_si_get_boolean(void *, const cli_string_option_t *);
bool cli_si_set_boolean(const char *, void *, const cli_string_option_t *);
char *cli_si_get_time_t(void *, const cli_string_option_t *);
bool cli_si_set_time_t(const char *, void *, const cli_string_option_t *);

#endif
