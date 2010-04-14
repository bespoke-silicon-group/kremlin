#!/usr/bin/python

# reads in region id to func_name map and region source line file
# and outputs the regions in a consolidated format with the following
# info: id, type (func/loop), module name, func name, start line, end line

import sys
import pickle

if len(sys.argv) != 4:
	print "Usage: %s <region_id to func_name file> <source line file> <output file>" % sys.argv[0]
	sys.exit(1)

func_name_file = open(sys.argv[1])
source_line_file = open(sys.argv[2])

# map of region id to func name string
func_name_map = {}

max_func_id = 0 # all regions with id higher than max_func_id are loops

for line in func_name_file:
	fields = line.split()
	region_id = int(fields[0])

	func_name_map[region_id] = fields[1]

	if region_id > max_func_id: max_func_id = region_id

# map from static region id to list with static region info
static_region_info = {}

for line in source_line_file:
	# fields should have the following info:
	# region_id module_id start_line end_line
	# Note that module_id needs to be prettified
	fields = line.split()

	if len(fields) != 4:
		print "Source line file not of correct format"
		sys.exit(1)

	last_slash_idx = fields[1].rfind("/")
	if last_slash_idx == -1: last_slash_idx = 0
	else: last_slash_idx += 1

	first_dot_after_slash_idx = fields[1].find(".",last_slash_idx)

	if first_dot_after_slash_idx == -1: first_dot_after_slash_idx = len(fields[1])

	# note whether this is a loop or a function
	if int(fields[0]) > max_func_id: type = "loop"
	else: type = "func"

	mod_name = fields[1][last_slash_idx:first_dot_after_slash_idx]

	static_region_info[int(fields[0])] = [type,mod_name,int(fields[2]),int(fields[3])]

# list of all region ids for functions
func_region_ids = [id for id in static_region_info.keys() if id in func_name_map.keys()]

# list of all region ids that are loops
loop_region_ids = [id for id in static_region_info.keys() if id not in func_region_ids]

region_id_to_func_region_id = {}

# set func region id of all functions to their own id
for id in func_region_ids: region_id_to_func_region_id[id] = id

# attempt to find the encompassing function for all loop regions
for id in func_region_ids:
	start_line = static_region_info[id][2]
	end_line = static_region_info[id][3]

	for loop_id in loop_region_ids:
		# should be in the same module, and be between start and end line
		if static_region_info[id][1] == static_region_info[loop_id][1] and start_line <= static_region_info[loop_id][2] and end_line >= static_region_info[loop_id][3]:

			# if we already mapped this to something else, we have a big problem
			if loop_id in region_id_to_func_region_id.keys():
				print "This loop was already labeled as being part of another function!"
				sys.exit(1)

			region_id_to_func_region_id[loop_id] = id

# look for any loops that didn't get an assignment
missing_func_mapping = [id for id in static_region_info.keys() if id not in region_id_to_func_region_id.keys()]

if len(missing_func_mapping) != 0:
	print "did not find function for some loop regions"
	sys.exit(1)


# insert the function name into the static region info dictionary now
for key in static_region_info.keys():
	static_region_info[key].insert(2,func_name_map[region_id_to_func_region_id[key]])

output_file = open(sys.argv[3],'w')

for key in static_region_info.keys():
	entry = static_region_info[key]

	output_str = "%d %s %s %s %d %d\n" % (key,entry[0],entry[1],entry[2],entry[3],entry[4])
	output_file.write(output_str)
	#pickle.dump(static_region_info[entry],output_file)

output_file.close()
