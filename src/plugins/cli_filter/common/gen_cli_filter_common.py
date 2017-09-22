#!/usr/bin/env python
##############################################################################
#  gen_cli_filter_common.py - (very) simple parser of cli opt.h data structs
#                             and code generator
##############################################################################
#  Copyright (C) 2017 Regents of the University of California
#  Produced at Lawrence Berkeley National Laboratory
#  Written by Douglas Jacobsen <dmjacobsen@lbl.gov>
#  All rights reserved.
#
#  This file is part of SLURM, a resource management program.
#  For details, see <https://slurm.schedmd.com/>.
#  Please also read the included file: DISCLAIMER.
#
#  SLURM is free software; you can redistribute it and/or modify it under
#  the terms of the GNU General Public License as published by the Free
#  Software Foundation; either version 2 of the License, or (at your option)
#  any later version.
#
#  In addition, as a special exception, the copyright holders give permission
#  to link the code of portions of this program with the OpenSSL library under
#  certain conditions as described in each individual source file, and
#  distribute linked combinations including the two. You must obey the GNU
#  General Public License in all respects for all of the code used other than
#  OpenSSL. If you modify file(s) with this exception, you may extend this
#  exception to your version of the file(s), but you are not obligated to do
#  so. If you do not wish to do so, delete this exception statement from your
#  version.  If you delete this exception statement from all source files in
#  the program, then also delete it here.
#
#  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along
#  with SLURM; if not, write to the Free Software Foundation, Inc.,
#  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
##############################################################################

import os
import sys
import subprocess
import re

verbose = False
structRegex = re.compile(r'\s*(typedef\s+)?struct\s+(\S+)\s*{')

string_iface = [
	{ 'type': 'char', 'pointer': 1, 'read': 'cli_si_get_string', 'write': 'cli_si_set_string' },
	{ 'type': 'int', 'pointer': 0, 'read': 'cli_si_get_int', 'write': 'cli_si_set_int' },
	{ 'type': 'long', 'pointer': 0, 'read': 'cli_si_get_long', 'write': 'cli_si_set_long' },
	{ 'type': 'unsigned', 'pointer': 0, 'read': 'cli_si_get_unsigned', 'write': 'cli_si_set_unsigned' },
	{ 'type': 'uid_t', 'pointer': 0, 'read': 'cli_si_get_uid', 'write': "NULL" },
	{ 'type': 'gid_t', 'pointer': 0, 'read': 'cli_si_get_gid', 'write': "NULL" },
	{ 'type': 'int8_t', 'pointer': 0, 'read': 'cli_si_get_int8_t', 'write': 'cli_si_set_int8_t' },
	{ 'type': 'int16_t', 'pointer': 0, 'read': 'cli_si_get_int16_t', 'write': 'cli_si_set_int16_t' },
	{ 'type': 'int32_t', 'pointer': 0, 'read': 'cli_si_get_int32_t', 'write': 'cli_si_set_int32_t' },
	{ 'type': 'int64_t', 'pointer': 0, 'read': 'cli_si_get_int64_t', 'write': 'cli_si_set_int64_t' },
	{ 'type': 'uint8_t', 'pointer': 0, 'read': 'cli_si_get_uint8_t', 'write': 'cli_si_set_uint8_t' },
	{ 'type': 'uint16_t', 'pointer': 0, 'read': 'cli_si_get_uint16_t', 'write': 'cli_si_set_uint16_t' },
	{ 'type': 'uint32_t', 'pointer': 0, 'read': 'cli_si_get_uint32_t', 'write': 'cli_si_set_uint32_t' },
	{ 'type': 'uint64_t', 'pointer': 0, 'read': 'cli_si_get_uint64_t', 'write': 'cli_si_set_uint64_t' },
	{ 'type': '_Bool', 'pointer': 0, 'read': 'cli_si_get_boolean', 'write': 'cli_si_set_boolean' },
	{ 'type': 'mem_bind_type_t', 'pointer': 0, 'read': 'cli_si_get_unsigned', 'write': 'cli_si_set_unsigned' },
	{ 'type': 'cpu_bind_type_t', 'pointer': 0, 'read': 'cli_si_get_unsigned', 'write': 'cli_si_set_unsigned' },
	{ 'type': 'time_t', 'pointer': 0, 'read': 'cli_si_get_time_t', 'write': 'cli_si_set_time_t' },
	{ 'name': 'argv', 'count_field': 'argc', 'read': 'cli_si_get_stringarray', 'write': 'NULL' },
	{ 'name': 'script_argv', 'count_field': 'script_argc', 'read': 'cli_si_get_stringarray', 'write': 'NULL' },
	{ 'name': 'spank_job_env', 'count_field': 'spank_job_env_size', 'read': 'cli_si_get_stringarray', 'write': 'NULL'}
]

lua_iface = [
	{ 'type': 'char', 'pointer': 1, 'read': 'cli_lua_push_string', 'write': 'cli_lua_set_string' },
	{ 'type': 'int', 'pointer': 0, 'read': 'cli_lua_push_int', 'write': 'cli_lua_set_int' },
	{ 'type': 'long', 'pointer': 0, 'read': 'cli_lua_push_long', 'write': 'cli_lua_set_long' },
	{ 'type': 'unsigned', 'pointer': 0, 'read': 'cli_lua_push_unsigned', 'write': 'cli_lua_set_unsigned' },
	{ 'type': 'uid_t', 'pointer': 0, 'read': 'cli_lua_push_uid', 'write': "NULL" },
	{ 'type': 'gid_t', 'pointer': 0, 'read': 'cli_lua_push_gid', 'write': "NULL" },
	{ 'type': 'int8_t', 'pointer': 0, 'read': 'cli_lua_push_int8_t', 'write': 'cli_lua_set_int8_t' },
	{ 'type': 'int16_t', 'pointer': 0, 'read': 'cli_lua_push_int16_t', 'write': 'cli_lua_set_int16_t' },
	{ 'type': 'int32_t', 'pointer': 0, 'read': 'cli_lua_push_int32_t', 'write': 'cli_lua_set_int32_t' },
	{ 'type': 'int64_t', 'pointer': 0, 'read': 'cli_lua_push_int64_t', 'write': 'cli_lua_set_int64_t' },
	{ 'type': 'uint8_t', 'pointer': 0, 'read': 'cli_lua_push_uint8_t', 'write': 'cli_lua_set_uint8_t' },
	{ 'type': 'uint16_t', 'pointer': 0, 'read': 'cli_lua_push_uint16_t', 'write': 'cli_lua_set_uint16_t' },
	{ 'type': 'uint32_t', 'pointer': 0, 'read': 'cli_lua_push_uint32_t', 'write': 'cli_lua_set_uint32_t' },
	{ 'type': 'uint64_t', 'pointer': 0, 'read': 'cli_lua_push_uint64_t', 'write': 'cli_lua_set_uint64_t' },
	{ 'type': '_Bool', 'pointer': 0, 'read': 'cli_lua_push_boolean', 'write': 'cli_lua_set_boolean' },
	{ 'type': 'mem_bind_type_t', 'pointer': 0, 'read': 'cli_lua_push_unsigned', 'write': 'cli_lua_set_unsigned' },
	{ 'type': 'cpu_bind_type_t', 'pointer': 0, 'read': 'cli_lua_push_unsigned', 'write': 'cli_lua_set_unsigned' },
	{ 'type': 'time_t', 'pointer': 0, 'read': 'cli_lua_push_time_t', 'write': 'cli_lua_set_time_t' },
	{ 'name': 'argv', 'count_field': 'argc', 'read': 'cli_lua_stringarray', 'write': 'NULL' },
	{ 'name': 'script_argv', 'count_field': 'script_argc', 'read': 'cli_lua_stringarray', 'write': 'NULL' },
	{ 'name': 'spank_job_env', 'count_field': 'spank_job_env_size', 'read': 'cli_lua_stringarray', 'write': 'NULL'}
]

def read_cli_opt_header(filename, structs):
	ret = {}
	cmdline = "cpp"
	if "CPPFLAGS" in os.environ:
		cmdline += " %s" % os.environ['CPPFLAGS']
	cmdline += " %s" % filename
	if verbose:
		sys.stderr.write('cmdline: %s\n' % cmdline)
	proc = subprocess.Popen(cmdline, stdout=subprocess.PIPE,
		stderr=subprocess.PIPE, shell=True)
	stdout,stderr = proc.communicate()
	filt_lines = []
	if type(stdout) is bytes:
		stdout = stdout.decode("utf-8")
	lines = stdout.split('\n')
	for line in lines:
		if not line.startswith('#'):
			filt_lines.append(line)
	cprog = ' '.join(filt_lines)
	start_search = 0
	match = structRegex.search(cprog[start_search:])
	while match:
		if match.groups()[1] in structs:
			member_start = start_search + match.end() + 1
			member_string = cprog[member_start:]
			end_idx = member_string.find('}')
			member_string = member_string[:end_idx]
			members = member_string.split(';')
			for idx,member in enumerate(members):
				members[idx] = member.strip()
			members = [x for x in members if len(x) > 0]
			memberdict = {}
			for member in members:
				data = member.split()
				pointer_degree = 0
				name = data[-1]
				while name.startswith('*'):
					name = name[1:]
					pointer_degree += 1
				idx = name.find('[')
				if idx >= 0:
					pointer_degree += 1
					name = name[:idx]
				type_name = data[0]
				if type_name == 'enum':
					type_name = 'int'
				if type_name == 'struct':
					type_name = 'struct_%s' % data[1]
				while type_name.endswith('*'):
					type_name = type_name[:-1]
					pointer_degree += 1
				memberdict[name] = {
					'type': type_name,
					'pointer': pointer_degree
				}
			ret[match.groups()[1]] = memberdict
		start_search += match.end() + 1
		match = structRegex.search(cprog[start_search:])
	return ret

def gen_interface(structs, iface_types, type_name, var_suffix, include):
	for fname in include:
		print('#include "%s"' % fname)
	for struct in structs:
		mapped = []
		members = sorted(structs[struct].keys())
		for member in members:
			m_data = structs[struct][member]
			iface_data = None
			for type_data in iface_types:
				if 'type' in type_data and 'pointer' in type_data and \
					m_data['type'] == type_data['type'] and \
					m_data['pointer'] == type_data['pointer']:

					iface_data = type_data
				elif 'name' in type_data and member == type_data['name']:
					iface_data = type_data
			if iface_data is None:
				if verbose:
					sys.stderr.write("Skipping %s member %s, type "
						"missing\n" % (struct, member))
				continue
			count_field = 'NULL'
			if 'count_field' in iface_data:
				count_field = '"%s"' % iface_data['count_field']
			mapping = '\t{ "%s", offsetof(struct %s, %s), %s, %s, %s }' \
				% (member, struct, member, iface_data['read'],
				iface_data['write'], count_field)
			mapped.append(mapping)
		if len(mapped) == 0:
			continue
		print("%s %s_%s[] = {" % (type_name, struct, var_suffix))
		total = len(mapped)
		for idx,item in enumerate(mapped):
			endidx = len(item)
			while endidx > 71:
				endidx = item[:endidx].rfind(' ')
			if endidx == len(item):
				print("%s," % (item))
			if len(item) > 71:
				print("%s" % (item[:endidx]))
				print("\t\t%s," % (item[endidx:]))
		print("\t{ NULL, 0, NULL, NULL, NULL }")
		print('};')

def main(args):
	structs = []
	files = []
	found = {}
	iface = None
	for arg in args:
		if arg == "--string":
			iface = "string"
		elif arg == "--lua":
			iface = "lua"
		elif arg == "--verbose":
			verbose = True
		elif os.path.exists(arg):
			files.append(arg)
		else:
			structs.append(arg)
	for filename in files:
		local_found = read_cli_opt_header(filename, structs)
		for key in local_found:
			found[key] = local_found[key]
	if iface == "string":
		gen_interface(found, string_iface, 'cli_string_option_t', 'si', ['cli_filter_common.h'])
	elif iface == "lua":
		gen_interface(found, lua_iface, 'cli_lua_option_t', 'li', ['cli_filter_lua.h'])

if __name__ == "__main__":
	main(sys.argv[1:])

