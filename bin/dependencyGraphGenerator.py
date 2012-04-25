#!/usr/bin/env python
import sys
import copy

class Function:
	def __init__(self,name,raw_lines):
		self.name = name.strip()
		self.basic_blocks = []
		self.subregions = [] # list of top level regions
		self.top_level_blocks = [] # list of basic blocks that aren't in subregion
		self.name_to_node = dict()
		self.name_to_bb = dict()
		self.process_raw_lines(raw_lines)
		if len(self.basic_blocks) != 0:
			self.form_subregions()

	def process_raw_lines(self,raw_lines):
		curr_bb_name = ""
		raw_bb_lines = []
		bb_rank = 0

		for line in raw_lines:
			if len(line) == 0: continue
			elif "BB_BEGIN" in line:
				assert len(curr_bb_name) == 0, "starting new bb before ending previous one (%s)" % curr_bb_name
				curr_bb_name = line.split(': ')[1]
			elif "BB_END" in line:
				end_bb_name = line.split(': ')[1]
				assert curr_bb_name == end_bb_name, "ending bb (%s) that wasn't active bb (%s)" % (end_bb_name,curr_bb_name)
				new_bb = BasicBlock(curr_bb_name,self.name_to_node,raw_bb_lines,bb_rank)
				bb_rank = bb_rank + 1
				self.basic_blocks.append(new_bb)
				self.name_to_bb[new_bb.name] = new_bb
				curr_bb_name = ""
				raw_bb_lines = []
			else:
				raw_bb_lines.append(line)

	def form_subregions(self):
		region_stack = []

		entry_bb = self.basic_blocks[0]
		self.process_bb(entry_bb,region_stack)

	def process_bb(self,basic_block,region_stack):
		region_stack_copy = [reg for reg in region_stack]

		for region_id,region_type,event in basic_block.region_events:
			if event == "enter":
				reg = Region(region_id,region_type)

				if len(region_stack_copy) != 0:
					region_stack_copy[-1].subregions.append(reg)
				else:
					self.subregions.append(reg)

				region_stack_copy.append(reg)

			elif event == "exit":
				assert region_stack_copy[-1].id == region_id, "%s exited but %s is top of stack" % (region_id, region_stack_copy[-1].id)
				region_stack_copy.pop()
			else:
				assert False, "unknown event type: %s" % event

		if len(region_stack_copy) != 0:
			region_stack_copy[-1].basic_blocks.append(basic_block)
		else:
			self.top_level_blocks.append(basic_block)

		for active_region in region_stack_copy:
			if basic_block.rank < active_region.rank:
				active_region.rank = basic_block.rank

		basic_blocks_to_process = []
		for bb_name in basic_block.next_basic_blocks:
			bb = self.name_to_bb[bb_name]
			if not bb.marked_for_processing:
				basic_blocks_to_process.append(bb)
				bb.marked_for_processing = True

		for bb in basic_blocks_to_process:
			self.process_bb(bb,region_stack_copy)

	def write_dot_graph(self,filename_prefix):
		dot_filename = filename_prefix + "-" + self.name + ".dot"
		output_file = open(dot_filename,"w")

		output_file.write("digraph G {\n")
		output_file.write("\tcompound = true;") # allows edges to BBs

		# sort subregions and basic_blocks
		clusters = copy.copy(self.subregions)
		clusters.extend(self.top_level_blocks)
		sorted_clusters = sorted(clusters,key=lambda x: x.rank)

		for cluster in sorted_clusters:
			cluster.write_dot(output_file,1)

		for name,node in self.name_to_node.items():
			output_file.write("\t" + name + "[")

			if node.type == "CALL":
				output_file.write("label=\"" + name + "()")
				if node.callsite_id != "":
					output_file.write("\\nCS: " + node.callsite_id)
				output_file.write("\"")
			else:
				output_file.write("label=\"" + name + " : " + node.type + "\"")

			output_file.write(",shape=" + node.shape)
			output_file.write(",color=" + node.color)
			output_file.write("];\n")

		output_file.write("}\n")
		output_file.close()

def get_indent_string(indent_level):
	tabs = ["\t" for i in range(indent_level)]
	indent_str = "".join(tabs)
	return indent_str

class Rankable(object):
	def __init__(self,rank):
		self.rank = rank

class Region(Rankable):
	def __init__(self,id,type):
		self.id = id.strip()
		self.type = type.strip()
		self.subregions = []
		self.basic_blocks = []
		super(Region,self).__init__(1000000)

	def write_dot(self,file,indent_level):
		indent_base = get_indent_string(indent_level)
		file.write(indent_base + "subgraph cluster_" + self.id + " {\n")

		clusters = copy.copy(self.subregions)
		clusters.extend(self.basic_blocks)
		sorted_clusters = sorted(clusters,key=lambda x: x.rank)

		for cluster in sorted_clusters:
			cluster.write_dot(file,indent_level+1)

		indent_body = get_indent_string(indent_level+1)
		file.write("\n"); # pleasing-to-the-eye blank line after edges
		#file.write(indent_body + "style = filled;\n")
		if self.type == "1":
			file.write(indent_body + "color = orange;\n")
		else:
			file.write(indent_body + "color = blue;\n")

		file.write(indent_body + "label = \"")
		if self.type == "1":
			file.write("loop")
		elif self.type == "2":
			file.write("loop body")
		file.write(" (id: " + self.id + ")\";\n")

		file.write(indent_base + "}\n\n")

class BasicBlock(Rankable):
	def __init__(self,name,name_to_node,raw_lines,rank):
		self.name = name.strip().replace('.','_')
		super(BasicBlock,self).__init__(rank)
		self.nodes = []
		self.edges = []
		self.parent_region = None
		self.next_basic_blocks = []
		self.region_events = []
		self.callsite_name = "" # used for handling KPrepCall neatly
		self.arg_links = [] # used for adding edges on KLinkArg
		self.return_link = "" # used for handling KLinkReturn
		self.curr_store_num = 0 # used for uniquifying store node names
		self.control_dep_name = "" # for drawing control dep edges
		self.name_to_node = name_to_node
		self.marked_for_processing = False
		self.process_raw_lines(raw_lines)

	def write_dot(self,file,indent_level):
		indent_base = get_indent_string(indent_level)
		file.write(indent_base + "subgraph cluster_" + self.name + " {\n")

		indent_body = get_indent_string(indent_level+1)

		for src,dest in self.edges:
			file.write(indent_body + src.name + " -> " + dest.name + ";\n")

		if len(self.control_dep_name) != 0 and len(self.nodes) != 0:
			for node in self.nodes:
				if not isinstance(node,CallNode) or len(node.callsite_id) != 0:
					file.write(indent_body + self.control_dep_name + " -> " + node.name)
					file.write(" [lhead=cluster_" + self.name + ",style=dotted,weight=3.0];\n")
					break

		file.write("\n"); # pleasing-to-the-eye blank line after edges
		file.write(indent_body + "style = dashed;\n")
		file.write(indent_body + "label = \"BB: " + self.name + "\";\n")
		file.write(indent_base + "}\n\n")

	def process_raw_lines(self,raw_lines):
		for line in raw_lines:
			line = line.strip()
			if "CALL" in  line: self.process_call(line)
			elif "RETURN" in line: continue
			elif "TERMINATOR" in line: self.process_terminator(line)
			else: self.process_lib_call(line)

	def process_call(self,line):
		# strip off the CALL: part
		line = line[5:].strip()

		if "=" in line:
			line_splits = line.split('= ')
			if line.find('=') < line.find('('):
				line = line_splits[1]
			else:
				line = line_splits[0]

		called_func_name = line[:line.find('(')]
		assert called_func_name != "", "no called func name!"

		cs_name = ""
		if self.callsite_name != "":
			cs_name = self.callsite_name

		call_node = CallNode(called_func_name,cs_name)
		self.nodes.append(call_node)
		self.name_to_node[call_node.name] = call_node
		# add edges from linked args to call
		for idx in range(len(self.arg_links)):
			self.edges.append((self.name_to_node[self.arg_links[idx]],call_node))
		# add edge from call to return link if one exists
		if self.return_link != "":
			self.edges.append((call_node,self.name_to_node[self.return_link]))

		# clear out (now obsolete) callsite name, arg links, and return link
		self.callsite_name = ""
		self.arg_links = []
		self.return_link = ""

	def process_terminator(self,term_string):
		# strip off TERMINATOR: part
		term_string = term_string[11:].strip()

		term_string_splits = term_string.split()
		self.next_basic_blocks = [bb.replace('.','_') for bb in term_string_splits]

	def process_lib_call(self,line):
		""" Returns pair of func name and list of arg strings"""
		def parse_lib_call_string(call_string):
			open_paren_idx = call_string.find('(')
			called_func_name = call_string[0:open_paren_idx]
			args = call_string[open_paren_idx+1:-1]

			# split arg list string by comma
			arg_splits = map(lambda str: str.strip(),args.split(','))
			return (called_func_name,arg_splits)

		func_name, args = parse_lib_call_string(line)

		if "_KTimestamp" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS"
			self.nodes.append(dest_node)
			num_deps = int(args[1])
			for idx in range(num_deps):
				dependency_name = "Reg" + args[2+2*idx]
				self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KTimestamp0" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS0"
			self.nodes.append(dest_node)
		elif "_KTimestamp1" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS1"
			self.nodes.append(dest_node)
			dependency_name = "Reg" + args[1]
			self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KTimestamp2" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS2"
			self.nodes.append(dest_node)

			dep1_name = "Reg" + args[1]
			self.edges.append((self.name_to_node[dep1_name],dest_node))
			dep2_name = "Reg" + args[3]
			self.edges.append((self.name_to_node[dep2_name],dest_node))
		elif "_KLoad" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD"
			self.nodes.append(dest_node)

			num_deps = int(args[3])
			for idx in range(num_deps):
				dependency_name = "Reg" + args[4+idx]
				self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KLoad0" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD0"
			self.nodes.append(dest_node)
		elif "_KLoad1" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD1"
			self.nodes.append(dest_node)

			dependency_name = "Reg" + args[2]
			self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KLoad2" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD2"
			self.nodes.append(dest_node)

			dep1_name = "Reg" + args[2]
			self.edges.append((self.name_to_node[dep1_name],dest_node))
			dep2_name = "Reg" + args[3]
			self.edges.append((self.name_to_node[dep2_name],dest_node))
		elif "_KStore" == func_name:
			store_name = self.name + "_st_" + str(self.curr_store_num)
			self.curr_store_num = self.curr_store_num + 1
			store_node = Node(store_name,"ST")
			self.name_to_node[store_name] = store_node
			self.nodes.append(store_node)

			dependency_name = "Reg" + args[0]
			self.edges.append((self.name_to_node[dependency_name],store_node))
		elif "_KPhi" in func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "PHI"

			if "_KPhiAddCond" == func_name:
				dep_name = "Reg" + args[1]
				self.edges.append((self.name_to_node[dep_name],dest_node))
			"""
			if "Cond" not in func_name:
				num_conds = int(func_name[5])
				range_max = num_conds + 2 # +1 for src, +1 for range() not not inclusive of second arg
			elif "_KPhiCond4To1" == func_name:
				range_max = 5
			elif "_KPhiAddCond" == func_name:
				range_max = 2

			for idx in range(1,range_max):
				dep_name = "Reg" + args[idx]
				dep_node = self.name_to_node[dep_name]
				self.edges.append((dep_node,dest_node))
			"""

		elif "_KInduction" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "IND"
		elif "_KPrepCall" == func_name:
			if self.callsite_name != "": sys.exit("last callsite_name not cleared")
			self.callsite_name = args[0]
		elif "_KLinkArg" == func_name:
			self.arg_links.append("Reg" + args[0])
		elif "_KUnlinkArg" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "ARG"
		elif "_KLinkReturn" == func_name:
			if self.return_link != "": sys.exit("last return_link not cleared")
			self.return_link = "Reg" + args[0]
			ret_link_node = self.name_to_node[self.return_link]
			ret_link_node.type = "RET"
		# like KStore, this doesn't indicate what BB return happened in
		elif "_KReturn" == func_name:
			ret_node = None
			if "RET" not in self.name_to_node:
				ret_node = Node("RET","RET")
			else:
				ret_node = self.name_to_node["RET"]

			src_node = self.name_to_node["Reg" + args[0]]
			self.edges.append((src_node,ret_node))

		elif "_KPushCDep" == func_name:
			self.control_dep_name = "Reg" + args[0]

	 	# ignore function region enter/exit
		elif "_KEnterRegion" == func_name:
			if args[1] != "0":
				self.region_events.append((args[0],args[1],"enter"))
		elif "_KExitRegion" == func_name:
			if args[1] != "0":
				self.region_events.append((args[0],args[1],"exit"))
		elif "_KPrepRTable" == func_name:
			for idx in range(1,int(args[0])):
				node_name = "Reg" + str(idx)
				self.name_to_node[node_name] = Node(node_name,"UNDEF");

class Node(object):
	def __init__(self,name,type):
		self.name = name.strip()
		self.type = type.strip()

	def get_type(self):
		return self._type

	def set_type(self,type):
		self._type = type
		if "LD" in self._type:
			self.shape = "house"
		elif "ST" in type:
			self.shape = "invhouse"
		elif "MEM" in type:
			self.shape = "box"
		elif "CALL" in type:
			self.shape = "diamond"
		elif "PHI" in type:
			self.shape = "invtriangle"
		elif "IND" in type:
			self.shape = "egg"
		else:
			self.shape = "ellipse"

		if "LD" in type or "ST" == type:
			self.color = "red"
		elif "PHI" in type:
			self.color = "purple"
		else:
			self.color = "black"

	type = property(get_type,set_type)

class CallNode(Node):
	def __init__(self,name,callsite_id):
		self.name = name.strip()
		self.type = "CALL"
		self.callsite_id = callsite_id.strip()

def main(args):
	kdump_filename = args[0]
	kdump_file = open(kdump_filename,"r")

	kdump_basename = kdump_filename[kdump_filename.rfind('/')+1:]
	kdump_basename = kdump_basename[:kdump_basename.find('.')]

	curr_func_name = ""
	raw_func_lines = []

	for line in kdump_file:
		if line == "\n": continue
		elif "FUNCTION_BEGIN" in line:
			assert len(curr_func_name) == 0, "trying to start new func before ending old one (%s)" % curr_func_name
			curr_func_name = line.split(': ')[1]

		elif "FUNCTION_END" in line:
			end_func_name = line.split(': ')[1]
			assert curr_func_name == end_func_name, "ending function (%s) that isn't current one (%s)" % (end_func_name,curr_func_name)
			if len(raw_func_lines) != 0:
				func = Function(curr_func_name,raw_func_lines)
				func.write_dot_graph(kdump_basename)

			# clear out (now obsolete) func name and raw lines
			curr_func_name = ""
			raw_func_lines = []
		else:
			raw_func_lines.append(line)

if __name__ == "__main__":
	assert len(sys.argv) == 2, "usage: %s <kdump file>" % sys.argv[0]
	main(sys.argv[1:])
